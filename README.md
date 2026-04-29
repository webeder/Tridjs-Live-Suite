🎧 Tridjs Live Suite - Beta 1.0

O Tridjs Live Suite é um ecossistema de performance musical desenvolvido para integrar software de alta performance com o hardware inovador DJ Hands Free. Criado em C++ com a biblioteca JUCE, o software oferece uma interface intuitiva inspirada em padrões da indústria (como o Rekordbox) para DJs e produtores que buscam expressividade através de gestos.

🚀 O Projeto "DJ Hands Free" (Air Hand)
O software atua como o cérebro que processa os sinais enviados por um controlador customizado baseado no ESP32-S3. Através de sensores de distância e gestos (VL53L5CX e PAJ7620U2), o artista controla efeitos e disparos de áudio sem a necessidade de toque físico, permitindo uma performance visual e técnica única.

🛠️ Funcionalidades Principais
Controle MIDI Nativo: Comunicação de ultra baixa latência via USB Nativo do ESP32-S3.

Interface Multi-Layout: Alternância entre os modos DJ Hand Free, Mode DJ e Performance Mode através do menu superior.

Navegação Inteligente: Painel lateral retrátil automatizado que abre nas abas corretas (FX, RGB, Learn, Serial, Config) via atalhos de teclado ou menu.

Mapeamento MIDI Learn: Sistema integrado para vincular gestos aos parâmetros de áudio de forma rápida.

Gerenciamento de Energia: Otimização de hardware que desativa módulos de rádio (Wi-Fi/Bluetooth) para garantir estabilidade máxima nos sensores analógicos.
Comando,Ação
Ctrl + L,Abre aba de Learn (Mapeamento)
Ctrl + R,Abre aba de RGB (Iluminação)
Ctrl + F,Abre aba de FX (Efeitos)
Ctrl + T,Abre aba de FX Touch
"Ctrl + ,",Configurações de Serial/Midi/Hardware

⚖️ Licença e Termos de Uso
Este software é mantido pelo Coletivo TriDJs e segue as seguintes diretrizes:

Uso: Gratuito para uso pessoal.
Comercialização: É estritamente PROIBIDA a venda ou exploração comercial deste software.
Modificações: Em caso de forks ou modificações do código no GitHub, é obrigatório manter os créditos originais ao Coletivo TriDJs e aos autores DJ Exder e DJ Christian Mauro.
Privacidade (LGPD): O software não coleta, armazena ou transmite dados pessoais dos usuários. Toda a operação é local.

🤝 Contribua e Apoie
O Tridjs Live Suite é um projeto que visa fomentar a cultura e a tecnologia musical. Se você deseja apoiar o desenvolvimento de novos sensores e a manutenção do projeto, utilize o caminho oficial:

👉 Site Oficial: www.tridjs.com.br/doar

Equipe Técnica
Desenvolvedor Principal: Eder Quadros (DJ Exder)
Segurança da Informação: Cristian Mauro
Coletivo TriDJs: Tecnologia, Gestão e Fomento à Cultura

