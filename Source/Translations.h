#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <array>

// 0 = Espanol, 1 = English, 2 = Portugues (BR)
inline juce::String tr (const juce::String& es, int lang)
{
    static const std::map<juce::String, std::array<const char*, 3>> t =
    {
        // Barra superior y vistas
        { "Simple", { "Simple", "Simple", "Simples" } },
        { "Avanzado", { "Avanzado", "Advanced", "Avançado" } },
        { "Antes", { "Antes", "Before", "Antes" } },
        { "Despues", { "Después", "After", "Depois" } },
        { "Vocal Assist", { "Vocal Assist", "Vocal Assist", "Vocal Assist" } },
        { "Escuchando...", { "Escuchando...", "Listening...", "Ouvindo..." } },
        { "Deshacer Assist", { "Deshacer Assist", "Undo Assist", "Desfazer Assist" } },
        { "Guardar", { "Guardar", "Save", "Salvar" } },
        { "Nombre del preset", { "Nombre del preset", "Preset name", "Nome do preset" } },
        { "Fabrica", { "Fábrica", "Factory", "Fábrica" } },
        { "Usuario", { "Usuario", "User", "Usuário" } },
        { "Toca tu voz: analizando 15 s", { "Pulsa Analizar y canta o reproduce tu voz 15 segundos", "Press Analyse and sing or play your vocal for 15 seconds", "Aperte Analisar e cante ou toque sua voz por 15 segundos" } },
        { "Listo. Ajustes aplicados.", { "Listo. Ajustes aplicados.", "Done. Settings applied.", "Pronto. Ajustes aplicados." } },
        { "No se detecto voz. Prueba otra vez.", { "No se detectó voz. Prueba otra vez.", "No vocal detected. Try again.", "Nenhuma voz detectada. Tente de novo." } },
        { "Tonalidad detectada", { "Tonalidad detectada", "Detected key", "Tom detectado" } },

        // Pestanas
        { "EQ", { "EQ", "EQ", "EQ" } },
        { "Afinacion", { "Afinación", "Tune", "Afinação" } },
        { "Dinamica", { "Dinámica", "Dynamics", "Dinâmica" } },
        { "Tono", { "Tono", "Tone", "Tom" } },
        { "Espacio", { "Espacio", "Space", "Espaço" } },
        { "Caracter", { "Carácter", "Character", "Caráter" } },

        // Controles
        { "Entrada", { "Entrada", "Input", "Entrada" } },
        { "Salida", { "Salida", "Output", "Saída" } },
        { "Mezcla", { "Mezcla", "Mix", "Mix" } },
        { "Limitador", { "Limitador", "Limiter", "Limitador" } },
        { "Tonalidad", { "Tonalidad", "Key", "Tom" } },
        { "Escala", { "Escala", "Scale", "Escala" } },
        { "Velocidad", { "Velocidad", "Speed", "Velocidade" } },
        { "Cantidad", { "Cantidad", "Amount", "Quantidade" } },
        { "Humanizar", { "Humanizar", "Humanize", "Humanizar" } },
        { "Modo", { "Modo", "Mode", "Modo" } },
        { "Limpieza", { "Limpieza", "Clean", "Limpeza" } },
        { "Filtro graves", { "Filtro graves", "Low cut", "Corte de graves" } },
        { "Puerta", { "Puerta", "Gate", "Gate" } },
        { "Cuerpo", { "Cuerpo", "Body", "Corpo" } },
        { "Barro", { "Barro", "Mud", "Embolado" } },
        { "Presencia", { "Presencia", "Presence", "Presença" } },
        { "Brillo", { "Brillo", "Air", "Brilho" } },
        { "Frec", { "Frec", "Freq", "Freq" } },
        { "De-esser", { "De-esser", "De-esser", "De-esser" } },
        { "Compresion", { "Compresión", "Compression", "Compressão" } },
        { "Ratio", { "Ratio", "Ratio", "Ratio" } },
        { "Ataque", { "Ataque", "Attack", "Ataque" } },
        { "Liberacion", { "Liberación", "Release", "Release" } },
        { "Estilo", { "Estilo", "Style", "Estilo" } },
        { "Ganancia", { "Ganancia", "Makeup", "Ganho" } },
        { "Auto", { "Auto", "Auto", "Auto" } },
        { "Inclinacion", { "Inclinación", "Tilt", "Inclinação" } },
        { "Aire", { "Aire", "Air", "Ar" } },
        { "Color", { "Color", "Color", "Cor" } },
        { "Tipo", { "Tipo", "Type", "Tipo" } },
        { "Doblador", { "Doblador", "Doubler", "Dobrador" } },
        { "Reverb", { "Reverb", "Reverb", "Reverb" } },
        { "Tamano", { "Tamaño", "Size", "Tamanho" } },
        { "Oscuridad", { "Oscuridad", "Darkness", "Escuridão" } },
        { "Pre-delay", { "Pre-delay", "Pre-delay", "Pre-delay" } },
        { "Delay", { "Delay", "Delay", "Delay" } },
        { "Division", { "División", "Division", "Divisão" } },
        { "Repeticiones", { "Repeticiones", "Feedback", "Repetições" } },
        { "Ping-Pong", { "Ping-Pong", "Ping-Pong", "Ping-Pong" } },
        { "Ducking", { "Ducking", "Ducking", "Ducking" } },
        { "Reduccion", { "Reducción", "Reduction", "Redução" } },
        { "Nota", { "Nota", "Note", "Nota" } },
        { "Activa", { "Activa", "On", "Ligada" } },
        { "Mayor", { "Mayor", "Major", "Maior" } },
        { "Menor", { "Menor", "Minor", "Menor" } },
        { "Cromatica", { "Cromática", "Chromatic", "Cromática" } },
        { "Menor armonica", { "Menor armónica", "Harmonic minor", "Menor harmônica" } },
        { "Pentatonica menor", { "Pentatónica menor", "Minor pentatonic", "Pentatônica menor" } },
        { "Pentatonica mayor", { "Pentatónica mayor", "Major pentatonic", "Pentatônica maior" } },
        { "Limpio", { "Limpio", "Clean", "Limpo" } },
        { "Vintage", { "Vintage", "Vintage", "Vintage" } },
        { "Agresivo", { "Agresivo", "Aggressive", "Agressivo" } },
        { "Cinta", { "Cinta", "Tape", "Fita" } },
        { "Valvula", { "Válvula", "Tube", "Válvula" } },
        { "Drive", { "Drive", "Drive", "Drive" } },
        { "Idioma", { "Idioma", "Language", "Idioma" } },
        { "Afinacion activa", { "Afinación activa", "Tune on", "Afinação ligada" } },
        { "Analiza tu voz 15 s y ajusta la cadena", { "Analiza tu voz 15 s y ajusta la cadena", "Analyses your vocal for 15 s and sets the chain", "Analisa sua voz por 15 s e ajusta a cadeia" } },
        { "Rapido", { "Rápido", "Fast", "Rápido" } },
        { "Natural", { "Natural", "Natural", "Natural" } },
        { "Suave", { "Suave", "Soft", "Suave" } },
        { "Robot", { "Robot", "Robot", "Robô" } },
        { "Brillante", { "Brillante", "Bright", "Brilhante" } },
        { "Calido", { "Cálido", "Warm", "Quente" } },
        { "Oscuro", { "Oscuro", "Dark", "Escuro" } },
        { "Neutro", { "Neutro", "Neutral", "Neutro" } },
        { "Nube", { "Nube", "Cloud", "Nuvem" } },
        { "Plate", { "Plate", "Plate", "Plate" } },
        { "Sala", { "Sala", "Hall", "Sala" } },
        { "Cuarto", { "Cuarto", "Room", "Quarto" } },
        { "Lo-Fi", { "Lo-Fi", "Lo-Fi", "Lo-Fi" } },
        { "Compresor", { "Compresor", "Compressor", "Compressor" } },
        { "Doubler", { "Doubler", "Doubler", "Doubler" } },
        { "Graves", { "Graves", "Low", "Graves" } },
        { "Medios", { "Medios", "Mids", "Médios" } },
        { "Agudos", { "Agudos", "Highs", "Agudos" } },
        { "Saturacion", { "Saturación", "Saturation", "Saturação" } },
        { "Ancho", { "Ancho", "Width", "Largura" } },
        { "Analizador", { "Analizador", "Analyzer", "Analisador" } },
        { "Tiempo", { "Tiempo", "Time", "Tempo" } },
        { "Ritmo", { "Ritmo", "Rhythm", "Ritmo" } },
        { "Nivel", { "Nivel", "Level", "Nível" } },
        { "Voces", { "Voces", "Voices", "Vozes" } },
        { "MS", { "MS", "MS", "MS" } },
        { "Bypass", { "Bypass", "Bypass", "Bypass" } },
        { "Assist", { "Assist", "Assist", "Assist" } },
        { "Ajustes", { "Ajustes", "Settings", "Ajustes" } },
        { "Analisis de voz", { "Análisis de voz", "Vocal analysis", "Análise de voz" } },
        { "Cadena vocal profesional", { "Cadena vocal profesional", "Professional vocal chain", "Cadeia vocal profissional" } },
        { "Ayuda", { "Ayuda", "Help", "Ajuda" } },
        { "HelpTip", { "Doble clic restablece un control. Arrastra los valores.", "Double-click resets a control. Drag the values.", "Clique duplo restaura um controle. Arraste os valores." } },
        { "Cerrar", { "Cerrar", "Close", "Fechar" } },
        { "Frecuencia", { "Frecuencia", "Frequency", "Frequência" } },
        { "Voces doubler", { "Voces", "Voices", "Vozes" } },
        { "Modo", { "Modo", "Mode", "Modo" } },
        { "ST", { "ST", "ST", "ST" } },
        { "Deshacer", { "Deshacer", "Undo", "Desfazer" } },
        { "Rehacer", { "Rehacer", "Redo", "Refazer" } },
        { "Guardar preset", { "Guardar preset", "Save preset", "Salvar preset" } },
        { "Antes / Despues de toda la cadena", { "Escuchar la voz sin procesar", "Listen to the unprocessed vocal", "Ouvir a voz sem processamento" } },
        { "Compara dos ajustes", { "Compara dos ajustes (A y B)", "Compare two settings (A and B)", "Compare dois ajustes (A e B)" } },
        { "Solo", { "Solo", "Solo", "Solo" } },
        { "Analizar", { "Analizar", "Analyse", "Analisar" } },
        { "Cancelar", { "Cancelar", "Cancel", "Cancelar" } },
        { "Listo", { "Listo", "Done", "Pronto" } },
        { "AssistDesc", { "Elige el género: analiza tu voz y le pone la cadena profesional de ese estilo, ajustada a tu toma.", "Pick the genre: it analyses your vocal and applies that style's pro chain, fitted to your take.", "Escolha o gênero: analisa sua voz e aplica a cadeia profissional desse estilo, ajustada à sua gravação." } },
        { "Trap Hard-Tune", { "Trap Hard-Tune", "Trap Hard-Tune", "Trap Hard-Tune" } },
        { "Trap Melodico", { "Trap Melódico", "Melodic Trap", "Trap Melódico" } },
        { "Latin Trap", { "Latin Trap", "Latin Trap", "Latin Trap" } },
        { "Reggaeton", { "Reggaetón", "Reggaeton", "Reggaeton" } },
        { "Rap", { "Rap", "Rap", "Rap" } },
        { "Drill", { "Drill", "Drill", "Drill" } },
        { "R&B", { "R&B", "R&B", "R&B" } },
        { "Pop Urbano", { "Pop Urbano", "Urban Pop", "Pop Urbano" } },
        { "Balada", { "Balada", "Ballad", "Balada" } },
        { "Voz Hablada", { "Voz Hablada", "Spoken Voice", "Voz Falada" } },
        { "Serie", { "Serie FET+Opto", "Series FET+Opto", "Série FET+Opto" } },
        { "Elige el genero", { "Elige el género", "Choose the genre", "Escolha o gênero" } },
        { "Genero", { "Género", "Genre", "Gênero" } },
        { "Ajustando", { "Ajustando", "Fitting", "Ajustando" } },
    };

    auto it = t.find (es);
    if (it == t.end()) return es;
    return juce::String::fromUTF8 (it->second[(size_t) juce::jlimit (0, 2, lang)]);
}

// Mayusculas que respetan acentos (á -> Á, ç -> Ç ...)
inline juce::String upper (const juce::String& s)
{
    juce::String r;
    for (auto c : s)
    {
        switch ((int) c)
        {
            case 0xe1: c = 0xc1; break; case 0xe9: c = 0xc9; break; case 0xed: c = 0xcd; break;
            case 0xf3: c = 0xd3; break; case 0xfa: c = 0xda; break; case 0xf1: c = 0xd1; break;
            case 0xe3: c = 0xc3; break; case 0xf5: c = 0xd5; break; case 0xe7: c = 0xc7; break;
            case 0xea: c = 0xca; break; case 0xf4: c = 0xd4; break; case 0xe2: c = 0xc2; break;
            default:   c = juce::CharacterFunctions::toUpperCase (c); break;
        }
        r << juce::String::charToString (c);
    }
    return r;
}
