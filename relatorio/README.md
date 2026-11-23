# Relatório do projeto

**Transmissão de dados confiável via rádio**

IFMT · Campus Cuiabá – Octayde Jorge da Silva

Transmissão e Comunicação de Dados · Professor Ruy Barbosa

Enzo Aurélio, Felipe Enzo e Enzo Cristian

*Documento em elaboração pela equipe. Ao concluir a redação, salve o PDF final nesta pasta e acrescente o link no README principal.*

## 1. Objetivo e montagem

Descrevam o que os dois computadores trocaram, o papel de cada ESP32 e o caminho dos ACKs. Incluam os esquemas A e B, os modelos dos módulos, a alimentação, as ligações e a identificação do display usado.

<!-- Exemplo, após adicionar o arquivo: ![Circuito B com o display ligado](imagens/circuito-b.jpg) -->

## 2. Protocolo e implementação

Identifiquem a revisão do firmware e as versões das bibliotecas. Expliquem o formato do quadro, o preenchimento, um cálculo de FCS e uma retransmissão observada. Relacionem o exemplo aos bytes do log. O [README do projeto](../README.md) contém o exemplo de referência com o texto TCD.

## 3. Procedimento experimental

Registrem a distância medida, o ambiente, o sentido da transmissão, a mensagem ou imagem de entrada e o CRC-8 do quadro de exemplo. Para cada ensaio, descrevam como reproduzi-lo. Indiquem quando o erro foi injetado por software e as condições do ensaio com ruído físico.

## 4. Resultados e evidências

Guardem fotos e capturas em [imagens](imagens/) e registros de texto em [logs](logs/). Numerem cada figura e usem a legenda para explicar qual resultado ela demonstra. Incluam o texto no display, os dois lados da troca por rádio e a imagem reconstruída no laptop.

| Ensaio | Condição medida | Resultado observado | Figura ou log |
| --- | --- | --- | --- |
| Texto A → B e B → A | | | |
| CRC-8 de um quadro DATA | | | |
| Quadro corrompido e perda de ACK | | | |
| Distância superior a 1 m e ruído ambiental | | | |
| Imagem de referência e reconstruída | | | |

Para as imagens, anotem resolução, contagem de bytes e SHA-256 dos pixels em cada ponta. Para o ARQ, identifiquem uma tentativa que expirou, a retransmissão com a mesma sequência e a entrega do conteúdo uma única vez.

## 5. Conclusão e referências

Registrem o que os ensaios demonstraram, as limitações observadas e as fontes efetivamente consultadas. Indiquem a configuração usada na apresentação e o commit exato do código.
