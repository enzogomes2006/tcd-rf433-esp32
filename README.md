# Transmissão de dados confiável via rádio

Projeto final de **Transmissão e Comunicação de Dados (TCD)** do **IFMT · Campus Cuiabá – Octayde Jorge da Silva**. Dois computadores trocam texto e imagens monocromáticas por meio de dois ESP32. Cada computador usa USB apenas para conversar com a sua placa. Entre as placas, tanto os dados quanto as confirmações passam pelos módulos de **433 MHz**.

| Equipe | Professor | Entrega prevista no enunciado |
| --- | --- | --- |
| Enzo Aurélio, Felipe Enzo e Enzo Cristian | Ruy Barbosa | 26/11/2026 · demonstração presencial |

## Onde encontrar cada parte

| Arquivo ou pasta | Conteúdo |
| --- | --- |
| [Circuito A](firmware/CircuitoA/CircuitoA.ino) | Código completo da placa A. Envia e recebe dados pelo rádio. |
| [Circuito B](firmware/CircuitoB/CircuitoB.ino) | Código completo da placa B. Envia e recebe dados pelo rádio e controla o OLED SSD1306. |
| [Programas para os computadores](computador/) | Envio da imagem, reconstrução em PBM e PNG, dependências e uma imagem de exemplo. |
| [Enunciado da disciplina](docs/enunciado.pdf) | Proposta, formato dos quadros, critérios de aceitação e avaliação. |
| [Relatório em elaboração](relatorio/README.md) | Modelo para a equipe redigir. As pastas [imagens](relatorio/imagens/) e [logs](relatorio/logs/) recebem as evidências de bancada. |

## Montagem

São necessários dois computadores, duas placas ESP32 clássicas, dois transmissores e dois receptores de 433 MHz, protoboards, jumpers, dois cabos USB de dados e um display no circuito B. Cada placa usa **um TX e um RX** porque o ACK precisa voltar pelo rádio.

| Ligação | Circuito A | Circuito B |
| --- | --- | --- |
| DATA do transmissor RF | GPIO26 → TX | GPIO26 → TX |
| DATA do receptor RF | RX → GPIO27 | RX → GPIO27 |
| Referência elétrica | GND comum aos módulos locais | GND comum aos módulos locais e ao display |
| OLED SSD1306 128×64 I²C | — | SDA → GPIO21, SCL → GPIO22, VCC → 3V3, GND → GND |

Confiram VCC e a ordem dos pinos no modelo de cada módulo RF antes de alimentar a protoboard. A entrada do ESP32 usa lógica de 3,3 V. Identifiquem no relatório os modelos e as tensões utilizados nas duas montagens.

**Display da apresentação.** A equipe usou o OLED SSD1306 no circuito B. O enunciado especifica LCD. Confirmem com o professor a aceitação do OLED para a demonstração.

## Gravação e primeiro teste

1. Baixe o repositório nos dois computadores. Na Arduino IDE, instale **esp32 by Espressif Systems** pelo Gerenciador de Placas. A compilação deste projeto foi verificada com a versão **2.0.17** e a placa **ESP32 Dev Module**.
2. No computador B, instale **Adafruit SSD1306** e **Adafruit GFX Library** pelo Gerenciador de Bibliotecas. Aceite a instalação da dependência **Adafruit BusIO**.
3. Abra apenas o sketch da placa que vai gravar. Para o ESP32 A, abra `firmware/CircuitoA/CircuitoA.ino`. Para o ESP32 B, abra `firmware/CircuitoB/CircuitoB.ino`. Em cada computador, selecione **ESP32 Dev Module**, escolha a porta USB e clique em **Carregar**.
4. Abra os dois monitores seriais em **115200 baud**, com final de linha **Nova linha**. Execute `/teste` nas duas placas. No A, envie `TCD`. Observe o ACK no A, a mensagem completa no B e o texto no display. Depois, envie uma resposta do B para o A. Use um emissor por vez.

## Envio de uma imagem

Feche os monitores seriais para liberar as portas. Execute os comandos a partir da raiz do repositório. No Windows, `py` chama o Python 3. Em Linux ou macOS, use `python3`. Substitua as portas de exemplo pelas portas locais. Com `py -m serial.tools.list_ports`, é possível listar as portas após instalar as dependências.

**Nos dois computadores**, instale as dependências.

```powershell
py -m pip install -r computador/requirements.txt
```

**Primeiro, no computador B**, inicie a recepção e aguarde a mensagem de escuta.

```powershell
py computador/receber_imagem_TCD.py COM5 --saida relatorio/imagens/recebidas
```

**Depois, no computador A**, envie a imagem de exemplo.

```powershell
py computador/enviar_imagem_TCD.py COM19 computador/exemplos/tcd.png --size 64x32 --sem-dither
```

O emissor ajusta a imagem para 1 bit por pixel e salva um PBM de referência ao lado do arquivo de entrada. O receptor valida o CRC32 dos pixels e grava o PBM e o PNG. Compare o **SHA-256 dos pixels** impresso nos dois computadores. Em 64×32 são 256 bytes, distribuídos em 16 quadros de pixels, além do cabeçalho e do END. Em 128×64 são 1.024 bytes e 64 quadros de pixels. Para inverter o sentido, inicie o script receptor no A e o emissor no B.

## Como o protocolo funciona

O rádio usa UART a **1200 baud, 8N1**, com preâmbulo e codificação 4B6B implementados no firmware. Cada grupo de quatro bits vira um símbolo de seis bits transportado por um byte da UART. O quadro lógico tem 21 bytes antes dessa codificação.

| Campo | Bytes | Função |
| --- | ---: | --- |
| FLAG | 1 | `0x7E`, início do quadro |
| TYPE | 1 | `0` DATA, `1` ACK, `2` END |
| SEQ | 1 | Bit de sequência alternado entre 0 e 1 |
| LEN | 1 | Quantidade de bytes úteis, de 1 a 16 em DATA e 0 em ACK/END |
| DATA | 16 | Dados e preenchimento com zeros até o fim do campo |
| FCS | 1 | Verificação de TYPE, SEQ, LEN e dos 16 bytes de DATA |

**Cálculo do FCS.** Os dois sketches usam CRC-8 sobre os 19 bytes de TYPE, SEQ, LEN e DATA. O polinômio é `0x87`, com valor inicial zero, sem reflexão e sem XOR final. Para cada byte, o algoritmo aplica XOR ao acumulador e faz oito deslocamentos à esquerda, aplicando o polinômio quando o bit superior anterior vale 1. Para o primeiro DATA com o texto ASCII `TCD`, os bytes protegidos são `00 00 03 54 43 44` seguidos de 13 zeros. O CRC-8 é **0x16**.

**Recuperação.** No Stop-and-Wait ARQ, o emissor aguarda o ACK de cada DATA e também do END. O receptor descarta símbolos inválidos ou quadros com FCS incorreto. Ao expirar a espera de 5 segundos, o emissor repete o mesmo quadro. Um DATA já aceito recebe outro ACK sem duplicar o conteúdo. A V7 transmite duas cópias de cada ACK. A confirmação do END de uma imagem também exige tamanho e CRC32 corretos.

**Onde ler o código.** Em qualquer um dos dois sketches, procure `montarFrame` e `crc8` para o formato e a integridade, `receberFrame` para a validação, `enviarComARQ` para o reenvio e `processarFrameRecebido` para duplicatas e END. A função `setup` inicia a UART e, no circuito B, o display. O texto comporta até 512 bytes por mensagem. Se uma placa for desligada durante uma transferência, a outra continuará tentando. Reinicie ambas para começar uma nova sessão.

## Requisitos do PDF e roteiro de demonstração

| Requisito | Como demonstrar | Registro a incluir no relatório |
| --- | --- | --- |
| Dois ESP32, enlace exclusivamente RF e texto nos dois sentidos (pp. 1–2) | Mostrar os cabos USB locais, enviar texto A → B e responder B → A. | Esquemas, fotos dos circuitos e logs das duas pontas. |
| Exibição do texto no LCD (pp. 1 e 4) | Exibir uma mensagem no OLED e confirmar a substituição com o professor. | Foto legível do texto e identificação do display demonstrado. |
| FCS e ARQ (p. 3) | O PDF pede checksum na etapa inicial. A versão usada nos ensaios implementa CRC-8. Executar `/teste` e enviar `TCD`. | Cálculo do CRC-8 e sequência DATA → ACK → END → ACK. |
| Corrupção, descarte e recuperação (p. 3) | No emissor, enviar `/erro` e depois uma mensagem. No receptor, usar `/perderack` antes de outra mensagem. | Descarte por FCS, timeout, mesma sequência retransmitida e entrega única. |
| Distância superior a 1 m e presença de ruído (p. 4) | Medir a distância, anotar o ambiente e executar `/lote 3`. Usar `/status` nas duas placas. Em ensaio separado, usar `/ruido 3` para corrupção por software. | Distância medida, condição física, tentativas, mensagens recebidas e contadores de cada ensaio. |
| Reconstrução íntegra da imagem no laptop (p. 4) | Executar os scripts, abrir o PNG recebido e comparar o SHA-256 dos pixels. | Imagem de referência, imagem recebida, hashes e logs dos dois computadores. |
| Código no GitHub, relatório completo e demonstração presencial (pp. 4–5) | Registrar a revisão usada na bancada e apresentar a montagem em funcionamento. | Link do repositório e PDF final na pasta do relatório. |

**Estado da validação.** Os dois sketches V7 compilaram separadamente para ESP32 clássico. A conversa da equipe registra testes da V7 a **1,10 m** e transferências em **64×32** e **128×64**. O relatório fica aberto para anexar os registros originais e documentar o ensaio de ruído físico.

**Origem do material.** O firmware e os scripts partem da versão V7 desenvolvida com apoio do ChatGPT na conversa de trabalho da equipe. O repositório estudado na conversa foi [esp32-rf433-arq, de lxinaider](https://github.com/lxinaider/esp32-rf433-arq). O enunciado descreve os critérios de autoria e uso de IA na página 5.
