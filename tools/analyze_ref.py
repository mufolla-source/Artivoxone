#!/usr/bin/env python3
"""Mide una acapella de referencia con las mismas medidas que usa el Vocal Assist de ArtisVox One.
Uso: python3 -I analyze_ref.py archivo.mp3 [salida.json]"""
import sys, json, subprocess, numpy as np
from scipy.signal import lfilter

SR = 48000

def load(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "2", "-ar", str(SR), "-"],
                         capture_output=True, check=True).stdout
    x = np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)
    return x

def rbj(kind, f0, q, sr=SR):
    w = 2 * np.pi * f0 / sr; a = np.sin(w) / (2 * q); c = np.cos(w)
    if kind == "bp":   b = [a, 0, -a]; A = [1 + a, -2 * c, 1 - a]
    elif kind == "hp": b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]; A = [1 + a, -2 * c, 1 - a]
    elif kind == "lp": b = [(1 - c) / 2, 1 - c, (1 - c) / 2]; A = [1 + a, -2 * c, 1 - a]
    return np.array(b) / A[0], np.array(A) / A[0]

def db(e): return 10 * np.log10(e + 1e-20)

def yin(x, sr, fmin=70, fmax=1000, win=2048, hop=480, thr=0.15):
    taumax = int(sr / fmin); taumin = int(sr / fmax); W = win // 2
    f0 = []
    for start in range(0, len(x) - win - taumax, hop):
        seg = x[start:start + W + taumax]
        if np.sqrt(np.mean(seg[:W] ** 2)) < 10 ** (-45 / 20): f0.append(0); continue
        d = np.zeros(taumax + 1)
        for tau in range(1, taumax + 1):
            df = seg[:W] - seg[tau:tau + W]; d[tau] = np.dot(df, df)
        cm = d.copy(); cm[0] = 1; cs = np.cumsum(d[1:]); cm[1:] = d[1:] * np.arange(1, taumax + 1) / np.maximum(cs, 1e-12)
        best = -1
        for t in range(taumin, taumax - 1):
            if cm[t] < thr:
                while t + 1 < taumax and cm[t + 1] < cm[t]: t += 1
                best = t; break
        if best < 0: f0.append(0); continue
        s0, s1, s2 = cm[best - 1], cm[best], cm[best + 1]; den = 2 * (2 * s1 - s2 - s0)
        bt = best + (s2 - s0) / den if abs(den) > 1e-12 else best
        f0.append(sr / bt)
    return np.array(f0), hop / sr

def main(path):
    st = load(path)
    mono = st.mean(axis=1)
    out = {"archivo": path.split("/")[-1], "duracion_s": round(len(mono) / SR, 1)}

    # --- Niveles por bloques de 50 ms
    B = int(0.05 * SR); nb = len(mono) // B
    blocks = mono[:nb * B].reshape(nb, B)
    lev = db(np.mean(blocks ** 2, axis=1))
    noise = np.percentile(lev, 10)
    p95all = np.percentile(lev, 95)
    act = (lev > p95all - 15) & (lev > -60)        # bloques con voz (robusto en voces muy comprimidas)
    al = np.sort(lev[act])
    med, p90, p95 = np.median(al), np.percentile(al, 90), np.percentile(al, 95)
    peak = 20 * np.log10(np.max(np.abs(mono)) + 1e-12)
    out.update(nivel_mediano_dBFS=round(med, 1), p90_dBFS=round(p90, 1), pico_dBFS=round(peak, 1),
               rango_dinamico_dB=round(p95 - med, 1), ruido_fondo_dBFS=round(noise, 1),
               factor_cresta_dB=round(peak - 10 * np.log10(np.mean(blocks[act] ** 2) + 1e-20), 1),
               fraccion_con_voz=round(float(act.mean()), 2))

    # --- Bandas (mismos filtros que el plugin)
    bands = {"cuerpo_120": ("bp", 120, 1.0), "barro_320": ("bp", 320, 1.0), "presencia_3k2": ("bp", 3200, 1.2),
             "eses_7k": ("bp", 7000, 1.4), "aire_11k": ("hp", 11000, 0.707)}
    refs = {"cuerpo_120": -9, "barro_320": -8, "presencia_3k2": -17, "eses_7k": -22, "aire_11k": -31}
    mask = np.repeat(act, B); mask = np.concatenate([mask, np.zeros(len(mono) - len(mask), bool)])
    tot = np.sum(mono[mask] ** 2)
    rel = {}
    for k, (kind, f, q) in bands.items():
        b, a = rbj(kind, f, q); y = lfilter(b, a, mono)
        rel[k] = round(db(np.sum(y[mask] ** 2)) - db(tot), 1)
    out["bandas_rel_dB"] = rel
    out["desviacion_vs_ref_plugin_dB"] = {k: round(rel[k] - refs[k], 1) for k in rel}
    # sub-graves (restos del 808 si es una acapella separada con IA)
    b, a = rbj("lp", 60, 0.707); sub = lfilter(b, a, mono)
    out["sub60_rel_dB"] = round(db(np.sum(sub[mask] ** 2)) - db(tot), 1)

    # --- Estereo
    m = st.mean(axis=1); s = (st[:, 0] - st[:, 1]) / 2
    out["lado_vs_centro_dB"] = round(db(np.sum(s ** 2)) - db(np.sum(m ** 2)), 1)

    # --- Afinacion (sobre 90 s para que sea rapido)
    seg = mono[: min(len(mono), 90 * SR)]
    b, a = rbj("lp", 1200, 0.707); segf = lfilter(b, a, seg)
    f0, dt = yin(segf[::2], SR // 2)
    v = f0[f0 > 0]
    if len(v) > 50:
        midi = 69 + 12 * np.log2(v / 440); dev = (midi - np.round(midi)) * 100
        out["afinacion"] = {"tramas_con_tono": int(len(v)), "desvio_medio_cents": round(float(np.mean(np.abs(dev))), 1),
                            "dentro_10_cents_pct": round(float(np.mean(np.abs(dev) < 10) * 100), 1),
                            "dentro_25_cents_pct": round(float(np.mean(np.abs(dev) < 25) * 100), 1)}
        # transiciones: duracion de los cambios de nota (tramas con desvio > 25 cents entre notas estables)
        notes = np.round(midi); trans = []
        i = 0
        while i < len(notes) - 1:
            if notes[i + 1] != notes[i]:
                j = i + 1; n = 0
                while j < len(dev) and abs(dev[j]) > 20 and n < 30: j += 1; n += 1
                trans.append(n * dt * 2 * 1000); i = j
            else: i += 1
        if trans: out["afinacion"]["transicion_media_ms"] = round(float(np.median(trans)), 1)
        # tonalidad (Krumhansl)
        hist = np.zeros(12)
        for mm in midi: hist[int(np.round(mm)) % 12] += 1
        maj = np.array([6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88]); mnr = np.array([6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17])
        names = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]; best = (-2, "")
        for k in range(12):
            for prof, nm in ((maj, "Mayor"), (mnr, "Menor")):
                c = np.corrcoef(np.roll(hist, -k), prof)[0, 1]
                if c > best[0]: best = (c, f"{names[k]} {nm}")
        out["tonalidad"] = best[1]

    # --- Espacio: colas despues de las frases (caida de energia en 5 ms)
    B2 = int(0.005 * SR); n2 = len(mono) // B2
    e = db(np.mean(mono[:n2 * B2].reshape(n2, B2) ** 2, axis=1))
    tails, tailLev = [], []
    i = 40
    while i < n2 - 200:
        prev = np.median(e[i - 40:i])
        if prev > med - 6 and e[i] < prev - 12:          # final de frase
            seg_e = e[i:i + 200]                          # 1 s despues
            t = np.arange(len(seg_e)) * 0.005
            good = seg_e > noise + 3
            if good.sum() > 20:
                slope = np.polyfit(t[good][:120], seg_e[good][:120], 1)[0]
                if slope < -3: tails.append(-60 / slope)
                tailLev.append(np.median(seg_e[10:40]) - prev)
            i += 200
        else: i += 1
    if tails:
        out["espacio"] = {"colas_medidas": len(tails), "rt60_estimado_s": round(float(np.median(tails)), 2),
                          "nivel_cola_vs_voz_dB": round(float(np.median(tailLev)), 1)}
    return out

if __name__ == "__main__":
    r = main(sys.argv[1])
    print(json.dumps(r, indent=2, ensure_ascii=False))
    if len(sys.argv) > 2:
        open(sys.argv[2], "w").write(json.dumps(r, indent=2, ensure_ascii=False))
