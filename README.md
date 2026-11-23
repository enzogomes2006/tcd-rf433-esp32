# Transmissão de dados confiável via rádio

Projeto final de **Transmissão e Comunicação de Dados (TCD)** do **IFMT · Campus Cuiabá – Octayde Jorge da Silva**. Dois computadores trocam texto e imagens monocromáticas por meio de dois ESP32. Cada computador usa USB apenas para conversar com a sua placa. Entre as placas, tanto os dados quanto as confirmações passam pelos módulos de **433 MHz**.

| Equipe | Professor | Entrega prevista no enunciado |
| --- | --- | --- |
| Enzo Aurélio, Felipe Enzo e Enzo Cristian | Ruy Barbosa | 26/11/2026 · demonstração presencial |

## Onde encontrar cada parte

| Arquivo ou pasta | Conteúdo |
| --- | --- |
| [Circuito A](firmware/CircuitoA/CircuitoA.ino) e [Circuito B](firmware/CircuitoB/CircuitoB.ino) | Sketch a gravar em cada ESP32. Os dois podem transmitir e receber. O B também controla o display. |
| [Biblioteca do projeto](firmware/libraries/TcdRadio/src/TcdRadio.h) | Implementação comum da V7 organizada por função, incluindo rádio, quadros, ARQ e reconstrução da imagem. |
| [Configuração](firmware/libraries/TcdRadio/src/Configuracao.h) | Seleção de checksum ou CRC e de OLED ou LCD. |
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

**Display da apresentação.** O enunciado especifica LCD. O padrão do firmware reproduz o OLED SSD1306 da equipe. Para demonstrar com LCD, selecione `TCD_USAR_LCD 1` na configuração e use um HD44780 16×2 com adaptador PCF8574 no circuito B. O endereço padrão é `0x27` e pode ser ajustado em `TCD_ENDERECO_LCD`. Use SDA/SCL nos mesmos GPIOs e adequação de nível se o adaptador puxar as linhas para 5 V. O LCD mostra até 32 caracteres por vez. A imagem completa é reconstruída no computador em ambos os modos.

## Gravação e primeiro teste

1. Baixe o repositório nos dois computadores. Na Arduino IDE, instale **esp32 by Espressif Systems** pelo Gerenciador de Placas. A compilação deste projeto foi verificada com a versão **2.0.17** e a placa **ESP32 Dev Module**.
2. Copie a pasta inteira `firmware/libraries/TcdRadio` para `libraries` dentro da sua pasta de sketches. O caminho dessa pasta aparece em **Arquivo → Preferências**. Reabra a IDE. Ao mudar configurações, edite a **cópia instalada** da biblioteca em cada computador.
3. No computador B, instale **Adafruit SSD1306** e **Adafruit GFX Library**, incluindo a dependência **Adafruit BusIO**. Para a opção LCD, instale **LiquidCrystal_PCF8574**, de Matthias Hertel.
4. No computador A, abra `CircuitoA.ino`. No B, abra `CircuitoB.ino`. Selecione a placa e a porta USB correspondentes e clique em **Carregar**. Use o mesmo modo de FCS nas duas placas.
5. Abra os dois monitores seriais em **115200 baud**, com final de linha **Nova linha**. Execute `/teste` nas duas placas. No A, envie `TCD`. Observe o ACK no A, a mensagem completa no B e o texto no display. Depois, envie uma resposta do B para o A. Use um emissor por vez.

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

**Cálculo do FCS.** A etapa inicial solicitada no PDF pode ser demonstrada selecionando `TCD_USAR_CHECKSUM 1` e regravando as duas placas. A soma dos 19 bytes protegidos é reduzida módulo 256. No padrão `0`, o firmware usa CRC-8 com polinômio `0x87`, valor inicial zero, sem reflexão e sem XOR final. Para cada byte, aplica XOR ao acumulador e faz oito deslocamentos à esquerda, aplicando o polinômio quando o bit superior anterior vale 1. Para o primeiro DATA com o texto ASCII `TCD`, os bytes protegidos são `00 00 03 54 43 44` seguidos de 13 zeros. O checksum é **0xDE** e o CRC-8 é **0x16**.

**Recuperação.** No Stop-and-Wait ARQ, o emissor aguarda o ACK de cada DATA e também do END. O receptor descarta símbolos inválidos ou quadros com FCS incorreto. Ao expirar a espera de 5 segundos, o emissor repete o mesmo quadro. Um DATA já aceito recebe outro ACK sem duplicar o conteúdo. A V7 transmite duas cópias de cada ACK. A confirmação do END de uma imagem também exige tamanho e CRC32 corretos.

**Onde ler o código.** Procure `montarFrame` e `calcularFCS` para o formato e a integridade, `receberFrame` para a validação, `enviarComARQ` para o reenvio e `processarFrameRecebido` para duplicatas e END. A UART e o display são inicializados em `iniciarCircuito`. O texto comporta até 512 bytes por mensagem. Se uma placa for desligada durante uma transferência, a outra continuará tentando. Reinicie ambas para começar uma nova sessão.

## Requisitos do PDF e roteiro de demonstração

| Requisito | Como demonstrar | Registro a incluir no relatório |
| --- | --- | --- |
| Dois ESP32, enlace exclusivamente RF e texto nos dois sentidos (pp. 1–2) | Mostrar os cabos USB locais, enviar texto A → B e responder B → A. | Esquemas, fotos dos circuitos e logs das duas pontas. |
| Exibição do texto no LCD (pp. 1 e 4) | Usar o perfil LCD ou confirmar com o professor a aceitação do OLED usado na montagem. | Foto legível do texto e identificação do display demonstrado. |
| FCS inicial por checksum, opção de CRC e ARQ (p. 3) | Gravar o mesmo perfil nas placas, conferir o exemplo `TCD` e repetir com o outro FCS. | Configurações, cálculo e sequência DATA → ACK → END → ACK. |
| Corrupção, descarte e recuperação (p. 3) | No emissor, enviar `/erro` e depois uma mensagem. No receptor, usar `/perderack` antes de outra mensagem. | Descarte por FCS, timeout, mesma sequência retransmitida e entrega única. |
| Distância superior a 1 m e presença de ruído (p. 4) | Medir a distância, anotar o ambiente e executar `/lote 3`. Usar `/status` nas duas placas. Em ensaio separado, usar `/ruido 3` para corrupção por software. | Distância medida, condição física, tentativas, mensagens recebidas e contadores de cada ensaio. |
| Reconstrução íntegra da imagem no laptop (p. 4) | Executar os scripts, abrir o PNG recebido e comparar o SHA-256 dos pixels. | Imagem de referência, imagem recebida, hashes e logs dos dois computadores. |
| Código no GitHub, relatório completo e demonstração presencial (pp. 4–5) | Registrar a revisão usada na bancada e apresentar a montagem em funcionamento. | Link do repositório e PDF final na pasta do relatório. |

**Estado da validação.** Os dois sketches compilaram para ESP32 clássico com CRC-8 e com checksum. No B foram compilados o perfil OLED e o perfil LCD. Os ensaios locais verificaram FCS, duplicatas e reconstrução de imagens em software. A conversa da equipe registra testes da V7 a **1,10 m** e transferências em **64×32** e **128×64**. O relatório fica aberto para a equipe anexar os registros originais, documentar o ensaio de ruído físico e identificar qual configuração foi demonstrada presencialmente.

**Origem do material.** O firmware e os scripts partem da versão V7 desenvolvida com apoio do ChatGPT na conversa de trabalho da equipe. Esta organização reúne o protocolo comum e acrescenta os perfis de checksum e LCD. O repositório estudado na conversa foi [esp32-rf433-arq, de lxinaider](https://github.com/lxinaider/esp32-rf433-arq). O enunciado descreve os critérios de autoria e uso de IA na página 5.
