/*
  TCD - RF433 - V7 (texto + imagens PBM 128x64 + ACK e CRC)
  Hardware validado pelos integrantes:
    RX RF DATA -> ESP32 GPIO27
    TX RF DATA -> ESP32 GPIO26
    OLED (somente B): SDA GPIO21, SCL GPIO22,
                      VCC 3V3 do ESP32, GND comum.

  Protocolo: FLAG(1) TYPE(1) SEQ(1) LEN(1) DATA(16) FCS(1).
  Tipos: DATA=0, ACK=1, END=2. CRC-8: poly 0x87, init 0.
  4B6B com tabela identica nos dois ESP32. UART RF: 1200 8N1.

  O comando /erro no Monitor Serial injeta 1 erro de CRC na proxima
  mensagem (somente na primeira tentativa de 1 quadro DATA).
  O transmissor repete o MESMO quadro sem limite ate receber ACK.
  Para interromper, reinicie o ESP32; depois reinicie os dois lados
  para ressincronizar as sessoes, se necessario.

  Envie de um computador por vez para evitar colisao no canal half duplex.
  Os GPIOs apenas tentam silenciar o modulo TX ao ouvir: o efeito exato
  depende do modulo RF empregado. Testar a RF fisica e obrigatorio.
*/

#define NO_EH_B 1
#if NO_EH_B
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
#endif
#include <Arduino.h>
#include <cstring>

// ===== PARTE 1. PINOS E CONFIGURACOES =====
const char *const TAG_NO = "B V7";
constexpr int PINO_RX = 27;
constexpr int PINO_TX = 26;
constexpr uint32_t BAUD_RF = 1200;
constexpr uint32_t BAUD_USB = 115200;
constexpr uint32_t TIMEOUT_ACK_MS = 5000;
constexpr uint32_t TEMPO_FRAME_MS = 1600;
constexpr uint32_t ATRASO_ANTES_ACK_MS = 180;
constexpr uint32_t PAUSA_APOS_ACK_MS = 1200; // protege a 2a copia do ACK
constexpr uint8_t FLAG = 0x7E;
constexpr uint8_t PREAMBULO = 0x2A;
constexpr uint8_t QTD_PREAMBULO = 32;
constexpr uint8_t COPIAS_ACK = 2; // ACK repetido para melhorar retorno RF
constexpr size_t TAM_FRAME = 21;
constexpr size_t TAM_DADOS = 16;
constexpr size_t MAX_TEXTO = 512;
constexpr uint8_t T_DATA = 0;
constexpr uint8_t T_ACK = 1;
constexpr uint8_t T_END = 2;

const uint8_t COD_4B6B[16] = {
  0x0D, 0x0E, 0x13, 0x15,
  0x16, 0x19, 0x1A, 0x1C,
  0x23, 0x25, 0x26, 0x29,
  0x2A, 0x2C, 0x32, 0x34
};

enum : uint8_t {
  SEM_SINAL = 0, FRAME_CORTADO = 1, CODIGO_INVALIDO = 2,
  QUADRO_INVALIDO = 3, CRC_ERRADO = 4, FRAME_OK = 5
};

// ===== PARTE 2. ESTATISTICAS E ESTADO =====
struct Contadores {
  uint32_t framesEnviados = 0;
  uint32_t framesValidos = 0;
  uint32_t ackEnviados = 0;
  uint32_t ackRecebidos = 0;
  uint32_t retransmissoes = 0;
  uint32_t errosCRC = 0;
  uint32_t erros4B6B = 0;
  uint32_t framesCortados = 0;
  uint32_t quadrosInvalidos = 0;
  uint32_t duplicados = 0;
  uint32_t mensagensEnviadas = 0;
  uint32_t mensagensRecebidas = 0;
  uint32_t acksSuprimidosTeste = 0;
  uint32_t errosInjetadosTeste = 0;
  uint32_t lotesEnviados = 0;
  uint32_t imagensEnviadas = 0;
  uint32_t imagensRecebidas = 0;
  uint32_t falhasImagem = 0;
};

Contadores stats;
char textoRecebido[MAX_TEXTO + 1] = {0};
size_t recebidoLen = 0;
uint8_t seqEsperadaRX = 0;
bool rxFinalizado = false;
uint8_t seqUltimoEND = 0;
bool injetarErroProximoDATA = false;
uint32_t errosSimuladosPendentes = 0; // numero de DATA a corromper na tentativa inicial
bool omitirProximoACKdeDATA = false;   // omite TODAS as copias desse ACK (uma unica vez)
bool omitirProximoACKdeEND = false;    // permite comprovar END duplicado

#if NO_EH_B
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledPronto = false;
#endif


// ===== V7: IMAGEM 128x64 (1 bit/pixel, 1024 bytes) =====
// Aplicacao sobre os tipos DATA, ACK, END existentes.
// Primeiro DATA: "TCDIM1"(6), largura(1), altura(1), tamanho uint16 LE,
// CRC32 uint32 LE, 2 bytes reservados. Total: 16 bytes.
// Proximos DATA: blocos binarios de ate 16 bytes.
// END: receptor valida quantidade e CRC32; envia ACK e exporta via USB.
constexpr size_t TAM_IMAGEM = 1024; // capacidade maxima do buffer
const uint8_t MAGIC_IMG[6] = {'T','C','D','I','M','1'};
uint8_t imagemRxW = 0, imagemRxH = 0;
size_t imagemRxTotal = 0;
uint8_t imagemTxW = 0, imagemTxH = 0;
size_t imagemTxTotal = 0;
bool formatoImagemValido(unsigned w, unsigned h) {
  return (w == 32 || w == 64 || w == 128) &&
         (h == 16 || h == 32 || h == 64) && w <= 128 && h <= 64;
}
uint8_t imagemRecebida[TAM_IMAGEM] = {0};
size_t imagemRxLen = 0;
uint32_t imagemRxCRC = 0;
bool imagemRxAtiva = false;
bool imagemTxAtiva = false;
uint8_t imagemTxSeq = 0;
size_t imagemTxEnviados = 0;
uint32_t imagemTxCRC = 0;

uint16_t lerU16LE(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
void escreverU16LE(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
uint32_t lerU32LE(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
void escreverU32LE(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
uint32_t crc32Imagem(const uint8_t *p, size_t n) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    for (uint8_t j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0UL);
  }
  return crc ^ 0xFFFFFFFFUL;
}

void progressoImagemOLED() {
#if NO_EH_B
  if (!oledPronto) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("IMAGEM RF433");
  oled.setCursor(0, 16);
  oled.print("Recebendo...");
  oled.setCursor(0, 32);
  oled.print((unsigned)imagemRxLen);
  oled.print(" / ");
  oled.print((unsigned)imagemRxTotal);
  oled.print(" bytes");
  oled.display();
#endif
}

// Formato facil de capturar pelo Python do computador receptor.
// Dados somente apos o END validado, para nao atrapalhar o ACK RF.
void exportarImagemParaUSB() {
  Serial.printf("@IMG_BEGIN %u %u %u %08lX\n",
                imagemRxW, imagemRxH, (unsigned)imagemRxTotal, (unsigned long)imagemRxCRC);
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < imagemRxTotal; i += 16) {
    Serial.print("@IMG_DATA ");
    for (size_t j = 0; j < 16; ++j) {
      uint8_t b = imagemRecebida[i + j];
      Serial.write(hex[b >> 4]);
      Serial.write(hex[b & 15]);
    }
    Serial.println();
  }
  Serial.printf("@IMG_END %08lX\n", (unsigned long)imagemRxCRC);
}

void mostrarImagemOLED() {
#if NO_EH_B
  if (!oledPronto) return;
  oled.clearDisplay();
  // PBM usa bit=1 para preto; na OLED exibimos esses bits como acesos.
  oled.drawBitmap((128 - imagemRxW) / 2, (64 - imagemRxH) / 2,
                  imagemRecebida, imagemRxW, imagemRxH, SSD1306_WHITE);
  oled.display();
#endif
}

// ===== PARTE 3. OLED (B) / SAIDA SERIAL (A) =====
void atualizarTela(const char *estado) {
#if NO_EH_B
  if (!oledPronto) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);
  oled.setCursor(0, 0);
  oled.print("B RF433 | ");
  oled.println(estado);
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  if (recebidoLen == 0) {
    oled.setCursor(0, 17);
    oled.print("Aguardando mensagem");
  } else {
    // 6 linhas de 21 caracteres na tela; mostra parte final do texto.
    const size_t visiveis = 21 * 6;
    size_t primeiro = recebidoLen > visiveis ? recebidoLen - visiveis : 0;
    size_t j = 0;
    for (size_t i = primeiro; i < recebidoLen; ++i, ++j) {
      if (j % 21 == 0) oled.setCursor(0, 16 + 8 * (j / 21));
      oled.write(static_cast<uint8_t>(textoRecebido[i]));
    }
  }
  oled.display();
#else
  (void)estado;
#endif
}

// ===== PARTE 4. UART RF: COMUTACAO MEIO DUPLEX =====
void modoRecepcao() {
  Serial2.end();
  pinMode(PINO_TX, OUTPUT);
  digitalWrite(PINO_TX, LOW);
  Serial2.begin(BAUD_RF, SERIAL_8N1, PINO_RX, -1);
}

void modoTransmissao() {
  Serial2.end();
  Serial2.begin(BAUD_RF, SERIAL_8N1, PINO_RX, PINO_TX);
  delay(35);
}

void iniciarRadio() {
  // Configurar buffer antes do primeiro begin (ESP32 Arduino).
  Serial2.setRxBufferSize(1024);
  modoRecepcao();
}

// ===== PARTE 5. CRC-8 (POLINOMIO 0x87) =====
uint8_t crc8(const uint8_t *dados, size_t quantidade) {
  uint8_t crc = 0;
  for (size_t i = 0; i < quantidade; ++i) {
    crc ^= dados[i];
    for (uint8_t b = 0; b < 8; ++b) {
      if (crc & 0x80) crc = static_cast<uint8_t>((crc << 1) ^ 0x87);
      else crc = static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

// ===== PARTE 6. ENQUADRAMENTO DE 21 BYTES =====
void montarFrame(uint8_t *f, uint8_t tipo, uint8_t seq,
                 const uint8_t *dados, uint8_t tamanho) {
  memset(f, 0, TAM_FRAME);
  f[0] = FLAG;
  f[1] = tipo;
  f[2] = seq;
  f[3] = tamanho;
  if (dados != nullptr && tamanho > 0 && tamanho <= TAM_DADOS) {
    memcpy(f + 4, dados, tamanho);
  }
  f[20] = crc8(f + 1, 19);  // TYPE, SEQ, LEN, DATA[16]
}

// ===== PARTE 7. CODIFICACAO / ENVIO =====
void escrever4b6b(uint8_t b) {
  Serial2.write(COD_4B6B[b >> 4]);
  Serial2.write(COD_4B6B[b & 0x0F]);
}

void transmitirFrame(const uint8_t *f) {
  modoTransmissao();
  for (uint8_t i = 0; i < QTD_PREAMBULO; ++i) escrever4b6b(PREAMBULO);
  for (size_t i = 0; i < TAM_FRAME; ++i) escrever4b6b(f[i]);
  Serial2.flush();
  delay(5);
  modoRecepcao();
  ++stats.framesEnviados;
}

// ===== PARTE 8. LEITURA E VALIDACAO DE FRAMES =====
bool antesDoPrazo(uint32_t prazo) {
  return static_cast<int32_t>(millis() - prazo) < 0;
}

bool lerSimbolo(uint8_t &b, uint32_t prazo) {
  while (antesDoPrazo(prazo)) {
    if (Serial2.available() > 0) {
      b = static_cast<uint8_t>(Serial2.read());
      return true;
    }
    delay(1);
  }
  return false;
}

int dec4b6b(uint8_t simbolo) {
  for (int i = 0; i < 16; ++i)
    if (COD_4B6B[i] == simbolo) return i;
  return -1;
}

uint8_t receberFrame(uint8_t *f, uint32_t janelaMs) {
  const uint32_t prazoBusca = millis() + janelaMs;
  // Guarda o ultimo simbolo entre chamadas. Isso evita perder uma FLAG
  // quando seus dois simbolos atravessam o limite da janela de leitura.
  static uint8_t anterior = 0;
  static bool temAnterior = false;
  uint8_t simbolo = 0;
  while (lerSimbolo(simbolo, prazoBusca)) {
    // FLAG 0x7E -> dois simbolos 4B6B: 7 e E.
    if (temAnterior && anterior == COD_4B6B[7] &&
        simbolo == COD_4B6B[14]) {
      temAnterior = false;  // terminou a FLAG; reinicia busca apos o frame
      f[0] = FLAG;
      const uint32_t prazoFrame = millis() + TEMPO_FRAME_MS;
      for (size_t i = 1; i < TAM_FRAME; ++i) {
        uint8_t hi, lo;
        if (!lerSimbolo(hi, prazoFrame) || !lerSimbolo(lo, prazoFrame))
          return FRAME_CORTADO;
        int a = dec4b6b(hi), b = dec4b6b(lo);
        if (a < 0 || b < 0) return CODIGO_INVALIDO;
        f[i] = static_cast<uint8_t>((a << 4) | b);
      }
      if (f[1] > T_END || f[2] > 1 || f[3] > TAM_DADOS ||
          (f[1] != T_DATA && f[3] != 0) ||
          (f[1] == T_DATA && f[3] == 0)) return QUADRO_INVALIDO;
      if (crc8(f + 1, 19) != f[20]) return CRC_ERRADO;
      return FRAME_OK;
    }
    anterior = simbolo;
    temAnterior = true;
  }
  return SEM_SINAL;
}

const char *nomeResultado(uint8_t r) {
  switch (r) {
    case FRAME_CORTADO: return "FRAME_CORTADO";
    case CODIGO_INVALIDO: return "4B6B_INVALIDO";
    case QUADRO_INVALIDO: return "CABECALHO_INVALIDO";
    case CRC_ERRADO: return "CRC_ERRADO";
    case FRAME_OK: return "FRAME_OK";
    default: return "SEM_SINAL";
  }
}

void registrarErro(uint8_t resultado) {
  if (resultado == CRC_ERRADO) ++stats.errosCRC;
  else if (resultado == CODIGO_INVALIDO) ++stats.erros4B6B;
  else if (resultado == FRAME_CORTADO) ++stats.framesCortados;
  else if (resultado == QUADRO_INVALIDO) ++stats.quadrosInvalidos;
  Serial.printf("[%s] DESCARTADO: %s (sem ACK)\n", TAG_NO, nomeResultado(resultado));
  // Sem atualizar OLED: manter os dados anteriores, evitar atraso no ACK.
}

// ===== PARTE 9. CONFIRMACAO ACK =====
void enviarACK(uint8_t seq) {
  uint8_t f[TAM_FRAME];
  montarFrame(f, T_ACK, seq, nullptr, 0);
  // Deixa o outro ESP32 mudar de TX para RX.
  delay(ATRASO_ANTES_ACK_MS);
  // Copias identicas do ACK melhoram a chance de confirmacao
  // quando o retorno B -> A esta fraco. O emissor aguarda 1200 ms
  // apos o primeiro ACK valido para nao colidir com a 2a copia.
  for (uint8_t copia = 0; copia < COPIAS_ACK; ++copia) {
    transmitirFrame(f);
    ++stats.ackEnviados;
    if (copia + 1 < COPIAS_ACK) delay(35);
  }
  Serial.printf("[%s] ACK transmitido seq=%u copias=%u\n",
                TAG_NO, seq, COPIAS_ACK);
}


// ===== PARTE 10. RECEBER TEXTO OU IMAGEM, COM ACK =====
void processarFrameRecebido(const uint8_t *f) {
  const uint8_t tipo = f[1];
  const uint8_t seq = f[2];
  const uint8_t tamanho = f[3];

  if (tipo == T_ACK) {
    Serial.printf("[%s] ACK isolado seq=%u (ignorado)\n", TAG_NO, seq);
    return;
  }

  if (tipo == T_END) {
    if (rxFinalizado && seq == seqUltimoEND) {
      ++stats.duplicados;
      Serial.printf("[%s] END duplicado: reenviando ACK\n", TAG_NO);
      enviarACK(seq);
      return;
    }
    if (seq != seqEsperadaRX) {
      Serial.printf("[%s] END fora de sequencia (esperado=%u)\n", TAG_NO, seqEsperadaRX);
      return;
    }
    // Uma imagem incorreta nao pode ser confirmada como concluida.
    if (imagemRxAtiva) {
      const uint32_t calculado = crc32Imagem(imagemRecebida, imagemRxLen);
      if (imagemRxLen != imagemRxTotal || calculado != imagemRxCRC) {
        ++stats.falhasImagem;
        Serial.printf("[%s] IMG INVALIDA: bytes=%u CRC32=%08lX esperado=%08lX; END sem ACK\n",
          TAG_NO, (unsigned)imagemRxLen, (unsigned long)calculado,
          (unsigned long)imagemRxCRC);
        return;
      }
    }
    rxFinalizado = true;
    seqUltimoEND = seq;
    if (omitirProximoACKdeEND) {
      omitirProximoACKdeEND = false;
      ++stats.acksSuprimidosTeste;
      Serial.printf("[%s] TESTE: ACK END omitido\n", TAG_NO);
    } else {
      enviarACK(seq);
    }
    ++stats.mensagensRecebidas;
    if (imagemRxAtiva) {
      ++stats.imagensRecebidas;
      Serial.printf("[%s] IMAGEM %ux%u VALIDADA CRC32=%08lX\n",
        TAG_NO, imagemRxW, imagemRxH, (unsigned long)imagemRxCRC);
      mostrarImagemOLED();
      exportarImagemParaUSB();
    } else {
      Serial.printf("[%s] TEXTO COMPLETO (%u bytes): ", TAG_NO, (unsigned)recebidoLen);
      Serial.write(reinterpret_cast<const uint8_t *>(textoRecebido), recebidoLen);
      Serial.println();
      atualizarTela("FIM");
    }
    return;
  }

  // Novo DATA seq=0 apos um END inicia nova sessao.
  if (rxFinalizado) {
    if (seq != 0) return;
    rxFinalizado = false;
    recebidoLen = 0;
    textoRecebido[0] = '\0';
    seqEsperadaRX = 0;
    imagemRxAtiva = false;
    imagemRxLen = 0;
  }

  if (seq != seqEsperadaRX) {
    ++stats.duplicados;
    Serial.printf("[%s] DATA duplicado seq=%u; reenviando ACK\n", TAG_NO, seq);
    enviarACK(seq);
    return;
  }

  // Header de inicio da imagem: 16 bytes, mesmo frame DATA normal.
  if (recebidoLen == 0 && !imagemRxAtiva && tamanho == 16 &&
      memcmp(f + 4, MAGIC_IMG, 6) == 0) {
    const uint8_t w = f[10], h = f[11];
    const uint16_t total = lerU16LE(f + 12);
    const uint32_t crc = lerU32LE(f + 14);
    if (!formatoImagemValido(w, h) || total != ((unsigned)w * h) / 8) {
      Serial.printf("[%s] HEADER IMG: tamanho nao suportado=%lu (sem ACK)\n",
                    TAG_NO, (unsigned long)total);
      return;
    }
    imagemRxAtiva = true;
    imagemRxLen = 0;
    imagemRxW = w; imagemRxH = h;
    imagemRxTotal = total;
    imagemRxCRC = crc;
    memset(imagemRecebida, 0, TAM_IMAGEM);
    Serial.printf("[%s] INICIO IMAGEM: %ux%u, %u bytes, CRC32=%08lX\n",
                  TAG_NO, w, h, total, (unsigned long)crc);
  } else if (imagemRxAtiva) {
    if (imagemRxLen + tamanho > imagemRxTotal) {
      Serial.printf("[%s] IMG excesso de bytes: sem ACK\n", TAG_NO);
      return;
    }
    memcpy(imagemRecebida + imagemRxLen, f + 4, tamanho);
    imagemRxLen += tamanho;
    if ((imagemRxLen % 128) == 0 || imagemRxLen == imagemRxTotal)
      Serial.printf("[%s] IMG recebida: %u/%u bytes\n",
                    TAG_NO, (unsigned)imagemRxLen, (unsigned)imagemRxTotal);
  } else {
    if (recebidoLen + tamanho > MAX_TEXTO) {
      Serial.printf("[%s] Texto grande demais: sem ACK\n", TAG_NO);
      return;
    }
    memcpy(textoRecebido + recebidoLen, f + 4, tamanho);
    recebidoLen += tamanho;
    textoRecebido[recebidoLen] = '\0';
  }

  seqEsperadaRX ^= 1;
  if (omitirProximoACKdeDATA) {
    omitirProximoACKdeDATA = false;
    ++stats.acksSuprimidosTeste;
    Serial.printf("[%s] TESTE ACK DATA omitido; seq=%u\n", TAG_NO, seq);
  } else {
    enviarACK(seq);
  }
  if (!imagemRxAtiva) {
    Serial.printf("[%s] TEXTO PARCIAL (%u bytes): %s\n",
      TAG_NO, (unsigned)recebidoLen, textoRecebido);
    atualizarTela("RECEBENDO");
  } else if (imagemRxLen == 0 || imagemRxLen % 128 == 0 || imagemRxLen == imagemRxTotal) {
    progressoImagemOLED();
  }
}

// ===== PARTE 11. ESPERAR ACK, SEM PARAR DE OUVIR DADOS =====
bool esperarACK(uint8_t seq) {
  const uint32_t fim = millis() + TIMEOUT_ACK_MS;
  while (antesDoPrazo(fim)) {
    const uint32_t tempoRestante = fim - millis();
    const uint32_t janela = tempoRestante > 450 ? 450 : tempoRestante;
    uint8_t f[TAM_FRAME] = {0};
    uint8_t r = receberFrame(f, janela);
    if (r == SEM_SINAL) continue;
    if (r != FRAME_OK) {
      registrarErro(r);
      continue;
    }
    ++stats.framesValidos;
    if (f[1] == T_ACK) {
      if (f[2] == seq) {
        ++stats.ackRecebidos;
        Serial.printf("[%s] ACK VALIDO seq=%u\n", TAG_NO, seq);
        return true;
      }
      Serial.printf("[%s] ACK atrasado seq=%u (queria %u)\n",
                    TAG_NO, f[2], seq);
    } else {
      // Durante uma transmissao propria, ainda responde a quadros
      // recebidos da outra ponta (sem recursao de send/ACK).
      processarFrameRecebido(f);
    }
  }
  return false;
}

// ===== PARTE 12. STOP-AND-WAIT COM REPETICAO INFINITA =====
bool enviarComARQ(const uint8_t *original) {
  uint32_t tentativa = 0;
  while (true) {
    ++tentativa;
    // Descarta residuos dos ACKs antigos antes desta tentativa.
    while (Serial2.available() > 0) Serial2.read();
    uint8_t envio[TAM_FRAME];
    memcpy(envio, original, TAM_FRAME);

    // Injeta UM erro real no frame de DATA, apenas na tentativa 1.
    // Nao recalcula o CRC: receptor precisa rejeitar e nao enviar ACK.
    if (envio[1] == T_DATA && tentativa == 1 &&
        (injetarErroProximoDATA || errosSimuladosPendentes > 0)) {
      if (injetarErroProximoDATA) injetarErroProximoDATA = false;
      else --errosSimuladosPendentes;
      envio[4] ^= 0x01; // altera 1 bit, NAO recalcula o FCS
      ++stats.errosInjetadosTeste;
      Serial.printf("[%s] ERRO_INJETADO (SIMULADO): DATA alterado, CRC mantido; restantes=%lu\n",
                    TAG_NO, (unsigned long)errosSimuladosPendentes);
    }
    if (tentativa > 1) ++stats.retransmissoes;
    Serial.printf("[%s] TX tipo=%u seq=%u tentativa=%lu\n", TAG_NO,
                  original[1], original[2], static_cast<unsigned long>(tentativa));
    transmitirFrame(envio);
    if (esperarACK(original[2])) {
      delay(PAUSA_APOS_ACK_MS);  // OLED/Serial podem concluir atualizacao
      return true;
    }
    Serial.printf("[%s] TIMEOUT/ACK perdido: repetindo MESMO frame\n", TAG_NO);
    // Prazos levemente diferentes para evitar colisoes A/B.
    delay(NO_EH_B ? 370 : 170);
  }
}

// ===== PARTE 13. TRANSMISSAO DE TEXTO LONGO =====
bool transmitirTexto(const char *texto, size_t quantidade) {
  if (quantidade == 0 || quantidade > MAX_TEXTO) return false;
  Serial.printf("[%s] INICIANDO TEXTO (%u bytes) em blocos de ate 16\n",
                TAG_NO, static_cast<unsigned>(quantidade));
  atualizarTela("ENVIANDO");
  uint8_t seq = 0;
  for (size_t p = 0; p < quantidade; p += TAM_DADOS) {
    uint8_t n = static_cast<uint8_t>(
      quantidade - p > TAM_DADOS ? TAM_DADOS : quantidade - p);
    uint8_t f[TAM_FRAME];
    montarFrame(f, T_DATA, seq,
                reinterpret_cast<const uint8_t *>(texto + p), n);
    enviarComARQ(f);
    seq ^= 1;
  }
  uint8_t fim[TAM_FRAME];
  montarFrame(fim, T_END, seq, nullptr, 0);
  enviarComARQ(fim);
  ++stats.mensagensEnviadas;
  Serial.printf("[%s] MENSAGEM ENVIADA COM END CONFIRMADO\n", TAG_NO);
  atualizarTela("ENVIADO");
  return true;
}


// ===== V7. COMANDOS USB PARA ENVIO DE IMAGEM BINARIA =====
// Host Python usa: /imgstart W H CRC32HEX -> /imghex 32HEX -> /imgend
int valorHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

void iniciarEnvioImagem(const char *param) {
  if (imagemTxAtiva) {
    Serial.println("@PC_ERROR imagem em andamento");
    return;
  }
  // Exemplo: /imgstart 64 32 1234ABCD
  unsigned w = 0, h = 0;
  unsigned long crc = 0;
  char extra = 0;
  if (sscanf(param, "%u %u %lx %c", &w, &h, &crc, &extra) != 3 ||
      !formatoImagemValido(w, h)) {
    Serial.println("@PC_ERROR use /imgstart 64 32 CRC32_HEX (w=32/64/128; h=16/32/64)");
    return;
  }
  uint8_t hdr[16] = {0};
  memcpy(hdr, MAGIC_IMG, 6);
  hdr[6] = (uint8_t)w; hdr[7] = (uint8_t)h;
  const uint16_t total = (uint16_t)((w * h) / 8);
  escreverU16LE(hdr + 8, total);
  escreverU32LE(hdr + 10, static_cast<uint32_t>(crc));
  uint8_t frame[TAM_FRAME];
  montarFrame(frame, T_DATA, 0, hdr, 16);
  Serial.printf("[%s] IMAGE TX inicio: %ux%u, CRC32=%08lX\n", TAG_NO, w, h, crc);
  enviarComARQ(frame); // Protocolo de tentativas infinitas, como V6.
  imagemTxAtiva = true;
  imagemTxSeq = 1;
  imagemTxEnviados = 0;
  imagemTxW = (uint8_t)w; imagemTxH = (uint8_t)h;
  imagemTxTotal = total;
  imagemTxCRC = static_cast<uint32_t>(crc);
  Serial.println("@PC_READY");
}

void enviarBlocoImagem(const char *hex) {
  if (!imagemTxAtiva) {
    Serial.println("@PC_ERROR envie /imgstart primeiro");
    return;
  }
  const size_t n = strlen(hex);
  if (n < 2 || n > 32 || (n & 1) || imagemTxEnviados + (n / 2) > imagemTxTotal) {
    Serial.println("@PC_ERROR bloco invalido (ate 16 bytes em hex)");
    return;
  }
  uint8_t bloco[16];
  for (size_t i = 0; i < n / 2; ++i) {
    int a = valorHex(hex[i * 2]);
    int b = valorHex(hex[i * 2 + 1]);
    if (a < 0 || b < 0) {
      Serial.println("@PC_ERROR caractere nao hexadecimal");
      return;
    }
    bloco[i] = (uint8_t)((a << 4) | b);
  }
  uint8_t frame[TAM_FRAME];
  montarFrame(frame, T_DATA, imagemTxSeq, bloco, (uint8_t)(n / 2));
  enviarComARQ(frame);
  imagemTxEnviados += n / 2;
  imagemTxSeq ^= 1;
  Serial.printf("@PC_ACK %u\n", (unsigned)imagemTxEnviados);
}

void encerrarEnvioImagem() {
  if (!imagemTxAtiva || imagemTxEnviados != imagemTxTotal) {
    Serial.printf("@PC_ERROR faltam bytes: %u/%u\n",
       (unsigned)imagemTxEnviados, (unsigned)imagemTxTotal);
    return;
  }
  uint8_t frame[TAM_FRAME];
  montarFrame(frame, T_END, imagemTxSeq, nullptr, 0);
  enviarComARQ(frame);
  imagemTxAtiva = false;
  ++stats.mensagensEnviadas;
  ++stats.imagensEnviadas;
  Serial.printf("@PC_DONE %08lX\n", (unsigned long)imagemTxCRC);
}

// ===== PARTE 14. COMANDOS E LOGS =====
void imprimirStatus() {
  Serial.printf("\n[%s] ===== STATUS =====\n", TAG_NO);
  Serial.printf("TX frames=%lu | RX validos=%lu\n",
                (unsigned long)stats.framesEnviados,
                (unsigned long)stats.framesValidos);
  Serial.printf("ACK enviados=%lu | ACK recebidos=%lu\n",
                (unsigned long)stats.ackEnviados,
                (unsigned long)stats.ackRecebidos);
  Serial.printf("Retransmissoes=%lu | Duplicados=%lu\n",
                (unsigned long)stats.retransmissoes,
                (unsigned long)stats.duplicados);
  Serial.printf("CRC errado=%lu | 4B6B invalido=%lu\n",
                (unsigned long)stats.errosCRC,
                (unsigned long)stats.erros4B6B);
  Serial.printf("Frame cortado=%lu | Cabecalho invalido=%lu\n",
                (unsigned long)stats.framesCortados,
                (unsigned long)stats.quadrosInvalidos);
  Serial.printf("Mensagens enviadas=%lu | recebidas=%lu\n",
                (unsigned long)stats.mensagensEnviadas,
                (unsigned long)stats.mensagensRecebidas);
  Serial.printf("TESTE ACK omitidos=%lu | erros injetados=%lu | lotes finalizados=%lu\n",
                (unsigned long)stats.acksSuprimidosTeste,
                (unsigned long)stats.errosInjetadosTeste,
                (unsigned long)stats.lotesEnviados);
  Serial.printf("Proximo DATA com erro=%s | Erros simulados pendentes=%lu | ACK DATA omitido=%s | ACK END omitido=%s\n",
                injetarErroProximoDATA ? "SIM" : "NAO",
                (unsigned long)errosSimuladosPendentes,
                omitirProximoACKdeDATA ? "SIM" : "NAO",
                omitirProximoACKdeEND ? "SIM" : "NAO");
  Serial.printf("Imagens TX=%lu | RX=%lu | erros imagem=%lu\n",
     (unsigned long)stats.imagensEnviadas, (unsigned long)stats.imagensRecebidas,
     (unsigned long)stats.falhasImagem);
  Serial.println("=============================\n");
}

void imprimirAjuda() {
  Serial.println("Comandos do Monitor Serial (115200 / Nova linha):");
  Serial.println("  texto qualquer   -> envia mensagem por RF");
  Serial.println("  /erro            -> corrompe 1 DATA na primeira tentativa");
  Serial.println("  /ruido N         -> corrompe N quadros DATA (uma vez cada), TESTE SIMULADO");
  Serial.println("  /perderack       -> omite as 2 copias do ACK do proximo DATA recebido");
  Serial.println("  /perderend       -> omite as 2 copias do ACK do proximo END recebido");
  Serial.println("  /lote N          -> envia N mensagens numeradas (1 a 30)");
  Serial.println("  /zerarstats      -> zera os contadores (NAO reseta a sequencia RF)");
  Serial.println("  /status          -> mostra contadores de CRC/ACK/reenvio");
  Serial.println("  /teste           -> testa CRC e 4B6B localmente");
  Serial.println("  /imgstart W H HEXCRC32 -> inicia PBM por USB/Python");
  Serial.println("  /imghex HEX      -> envia ate 16 bytes binarios da imagem");
  Serial.println("  /imgend          -> encerra imagem e confirma END");
  Serial.println("  /ajuda           -> mostra esta ajuda");
  Serial.println("Somente uma ponta deve iniciar mensagem de cada vez.");
  Serial.println("Retransmissao sem limite: Reset no ESP32 para interromper.\n");
}

void testeLocal() {
  uint8_t f[TAM_FRAME];
  const uint8_t d[3] = {'T', 'C', 'D'};
  montarFrame(f, T_DATA, 0, d, 3);
  bool crcOK = (crc8(f + 1, 19) == f[20]);
  uint8_t crcOriginal = f[20];
  f[4] ^= 0x01;
  bool detectou = (crc8(f + 1, 19) != crcOriginal);
  bool tabelaOK = true;
  for (int i = 0; i < 16; ++i) {
    if (dec4b6b(COD_4B6B[i]) != i) tabelaOK = false;
  }
  Serial.printf("[%s] TESTE LOCAL: CRC=%s | CRC detectou alteracao=%s | 4B6B=%s\n",
                TAG_NO, crcOK ? "OK" : "FALHOU",
                detectou ? "OK" : "FALHOU",
                tabelaOK ? "OK" : "FALHOU");
}

// ===== ENSAIO REPETIVEL DE DISTANCIA / ESTABILIDADE =====
// Envia identificadores numerados para conferir no receptor quantos
// textos chegaram completos e se houve retransmissoes.
void executarLote(uint8_t quantidade) {
  Serial.printf("[%s] LOTE INICIO: %u mensagens (medir distancia fisicamente)\n",
                TAG_NO, quantidade);
  for (uint8_t i = 1; i <= quantidade; ++i) {
    char msg[100];
    snprintf(msg, sizeof(msg), "TCD %02u/%02u - RF433 TESTE LONGO",
             i, quantidade);
    Serial.printf("[%s] LOTE item %u/%u: %s\n", TAG_NO, i, quantidade, msg);
    if (!transmitirTexto(msg, strlen(msg))) {
      Serial.printf("[%s] LOTE interrompido no item %u\n", TAG_NO, i);
      return;
    }
  }
  ++stats.lotesEnviados;
  Serial.printf("[%s] LOTE CONCLUIDO: %u mensagens com END confirmado\n",
                TAG_NO, quantidade);
  imprimirStatus();
}

// Aceita inteiros decimais estritos no intervalo de 1 ate limite.
uint8_t lerQuantidadeComando(const char *p, uint8_t limite) {
  if (!p || !*p) return 0;
  unsigned n = 0;
  while (*p >= '0' && *p <= '9') {
    n = n * 10 + (unsigned)(*p - '0');
    if (n > limite) return 0;
    ++p;
  }
  return (!*p && n >= 1 && n <= limite) ? (uint8_t)n : 0;
}

// ===== PARTE 15. SERIAL USB =====
char linha[MAX_TEXTO + 1] = {0};
size_t linhaLen = 0;
bool linhaLonga = false;

void executarLinha() {
  linha[linhaLen] = '\0';
  if (linhaLonga) {
    Serial.printf("[%s] Mais de %u bytes: linha descartada\n", TAG_NO,
                  static_cast<unsigned>(MAX_TEXTO));
  } else if (strncmp(linha, "/imgstart ", 10) == 0) {
    iniciarEnvioImagem(linha + 10);
  } else if (strncmp(linha, "/imghex ", 8) == 0) {
    enviarBlocoImagem(linha + 8);
  } else if (strcmp(linha, "/imgend") == 0) {
    encerrarEnvioImagem();
  } else if (imagemTxAtiva) {
    Serial.println("@PC_ERROR finalize a imagem antes de texto/comandos");
  } else if (strcmp(linha, "/ajuda") == 0) {
    imprimirAjuda();
  } else if (strcmp(linha, "/status") == 0) {
    imprimirStatus();
  } else if (strcmp(linha, "/teste") == 0) {
    testeLocal();
  } else if (strcmp(linha, "/erro") == 0) {
    injetarErroProximoDATA = true;
    Serial.printf("[%s] Erro armado para o PROXIMO frame DATA\n", TAG_NO);
  } else if (strcmp(linha, "/perderack") == 0) {
    omitirProximoACKdeDATA = true;
    Serial.printf("[%s] TESTE PERDA ACK armado para o proximo DATA recebido\n", TAG_NO);
  } else if (strcmp(linha, "/perderend") == 0) {
    omitirProximoACKdeEND = true;
    Serial.printf("[%s] TESTE PERDA ACK END armado para o proximo END recebido\n", TAG_NO);
  } else if (strcmp(linha, "/zerarstats") == 0) {
    stats = Contadores{};
    Serial.printf("[%s] Contadores zerados; estado do protocolo preservado\n", TAG_NO);
  } else if (strncmp(linha, "/ruido ", 7) == 0) {
    uint8_t n = lerQuantidadeComando(linha + 7, 30);
    if (!n) Serial.println("Uso: /ruido N (N de 1 a 30)");
    else {
      errosSimuladosPendentes = n;
      Serial.printf("[%s] SIMULACAO preparada: corromper os proximos %u DATA (primeiro envio)\n", TAG_NO, n);
    }
  } else if (strncmp(linha, "/lote ", 6) == 0) {
    uint8_t n = lerQuantidadeComando(linha + 6, 30);
    if (!n) Serial.println("Uso: /lote N (N de 1 a 30)");
    else executarLote(n);
  } else if (linhaLen > 0) {
    transmitirTexto(linha, linhaLen);
    imprimirStatus();
  }
  linhaLen = 0;
  linhaLonga = false;
}

void lerUSB() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      executarLinha();
    } else if (!linhaLonga) {
      if (linhaLen < MAX_TEXTO) linha[linhaLen++] = c;
      else linhaLonga = true;
    }
  }
}

// ===== PARTE 16. SETUP E LOOP =====
void setup() {
  Serial.begin(BAUD_USB);
  delay(500);
#if NO_EH_B
  Wire.begin(OLED_SDA, OLED_SCL);
  uint8_t endereco = 0;
  for (uint8_t e = 0x3C; e <= 0x3D; ++e) {
    Wire.beginTransmission(e);
    if (Wire.endTransmission() == 0) { endereco = e; break; }
  }
  if (endereco) oledPronto = oled.begin(SSD1306_SWITCHCAPVCC, endereco);
  Serial.printf("[%s] OLED: %s (0x%02X)\n", TAG_NO,
                oledPronto ? "OK" : "NAO IDENTIFICADA", endereco);
  atualizarTela("AGUARDANDO");
#endif
  iniciarRadio();
  Serial.printf("\n[%s] INICIADO: GPIO27 RX / GPIO26 TX / RF1200 / USB115200\n", TAG_NO);
  Serial.println("PROTOCOLO V7: 4B6B + CRC8 + Stop-and-Wait + IMG 128x64");
  imprimirAjuda();
}

void loop() {
  lerUSB();
  uint8_t f[TAM_FRAME] = {0};
  uint8_t r = receberFrame(f, 150);
  if (r == FRAME_OK) {
    ++stats.framesValidos;
    processarFrameRecebido(f);
  } else if (r != SEM_SINAL) {
    registrarErro(r);
  }
  delay(1);
}
