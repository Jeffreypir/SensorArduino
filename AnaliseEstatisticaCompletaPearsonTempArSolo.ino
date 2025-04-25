/*
 * Autor: Jefferson Bezerra dos Santos 
 *
 * Sistema de Monitoramento Agrícola Inteligente
 * 
 * Dispositivo: Arduino Nano
 * Sensores: DHT11 (Temperatura/Umidade), Higrômetro Resistivo (Solo)
 * Armazenamento: Cartão SD (FAT32)
 * 
 * Funcionalidades:
 * 1. Leitura periódica de sensores (5 minutos)
 * 2. Cálculo estatístico completo (média, desvio padrão, quartis)
 * 3. Detecção de anomalias usando método IQR
 * 4. Análise de correlação entre variáveis
 * 5. Armazenamento em CSV com data/hora relativa
 * 6. Mudança automática de dias após 24 horas
 */

#include <DHT.h>
#include <SD.h>
#include <SPI.h>
#include <math.h>

// ================= CONFIGURAÇÕES DE HARDWARE =================
#define DHTPIN A1            // Pino digital para DHT11 (libera pino analógico)
#define DHTTYPE DHT11        // Modelo do sensor DHT
#define SOIL_PIN A0          // Pino analógico para sensor de solo
#define SD_CS_PIN 10         // Pino ChipSelect do módulo SD (padrão SPI)

// ================= PARÂMETROS DE AMOSTRAGEM =================
#define LOG_INTERVAL 300000  // Intervalo entre leituras (5 minutos em ms)
#define SAMPLE_SIZE 8        // Tamanho da amostra para estatísticas
#define CORR_BUFFER_SIZE 12  // Tamanho do buffer para correlação

// ================= CONSTANTES ESTATÍSTICAS =================
#define IQR_FACTOR 1.5       // Fator para detecção de outliers
#define CORR_THRESHOLD 0.5   // Limiar para correlação significativa

// ================= CALIBRAÇÃO SENSOR DE SOLO =================
#define SOIL_DRY_VALUE 1023  // Valor lido quando solo está seco
#define SOIL_WET_VALUE 300   // Valor lido quando solo está saturado

// ================= DATA DE REFERÊNCIA =================
#define REFERENCE_YEAR 2025
#define REFERENCE_MONTH 4
#define REFERENCE_DAY 23

// ================= HORÁRIO DE INÍCIO =================
#define START_HOUR 13
#define START_MINUTE 20

// ================= ESTRUTURAS DE DADOS =================

/**
 * @struct Estatisticas
 * @brief Armazena métricas estatísticas calculadas
 */
struct Estatisticas {
  float media;          // Média aritmética
  float desvio_padrao;  // Desvio padrão amostral
  float variancia;      // Variância amostral
  float q1;            // Primeiro quartil (25%)
  float mediana;       // Mediana (50%)
  float q3;            // Terceiro quartil (75%)
  float iqr;           // Intervalo interquartil (Q3-Q1)
  bool is_outlier;     // Indica se o último valor é outlier
};

/**
 * @struct ResultadoCorrelacao
 * @brief Armazena resultado da análise de correlação
 */
struct ResultadoCorrelacao {
  float coeficiente;    // Coeficiente de Pearson (-1 a 1)
  bool significativa;   // Se |coeficiente| > CORR_THRESHOLD
  const char* variavel1; // Nome da 1ª variável
  const char* variavel2; // Nome da 2ª variável
};

/**
 * @struct DadosSensores
 * @brief Armazena leituras atuais dos sensores
 */
struct DadosSensores {
  float temperatura;           // Em graus Celsius
  float umidade_ar;            // Umidade relativa (%)
  float umidade_solo_percent;  // Umidade do solo (0-100%)
};

// ================= VARIÁVEIS GLOBAIS =================
DHT dht(DHTPIN, DHTTYPE);  // Objeto para sensor DHT
File dataFile;             // Objeto para arquivo no SD

// Buffers circulares para estatísticas
float temp_samples[SAMPLE_SIZE] = {0};
float umid_ar_samples[SAMPLE_SIZE] = {0};
float umid_solo_samples[SAMPLE_SIZE] = {0};
byte sample_index = 0;

// Buffers para correlação
float temp_buffer_corr[CORR_BUFFER_SIZE] = {0};
float umid_ar_buffer_corr[CORR_BUFFER_SIZE] = {0};
float umid_solo_buffer_corr[CORR_BUFFER_SIZE] = {0};
byte corr_index = 0;

// ================= FUNÇÕES AUXILIARES =================

/**
 * @brief Ordena array usando Bubble Sort otimizado
 * @param arr Array para ordenar
 * @param n Tamanho do array
 */
void bubbleSort(float arr[], int n) {
  for (int i = 0; i < n-1; i++) {
    bool swapped = false;
    for (int j = 0; j < n-i-1; j++) {
      if (arr[j] > arr[j+1]) {
        float temp = arr[j];
        arr[j] = arr[j+1];
        arr[j+1] = temp;
        swapped = true;
      }
    }
    if (!swapped) break;
  }
}

/**
 * @brief Converte leitura analógica para % de umidade do solo
 * @param raw_value Valor lido (0-1023)
 * @return Porcentagem (0-100%)
 */
float mapSoilMoistureToPercent(int raw_value) {
  raw_value = constrain(raw_value, SOIL_WET_VALUE, SOIL_DRY_VALUE);
  return 100.0 - map(raw_value, SOIL_WET_VALUE, SOIL_DRY_VALUE, 0, 100);
}

// ================= FUNÇÕES ESTATÍSTICAS =================

/**
 * @brief Calcula estatísticas descritivas para um conjunto de amostras
 * @param samples Array de amostras
 * @param new_val Novo valor para verificar outlier
 * @return Estrutura Estatisticas com resultados
 */
Estatisticas calcularEstatisticas(float samples[], float new_val) {
  Estatisticas res;
  float sum = 0, sum_sq = 0;
  byte valid_count = 0;

  for (byte i = 0; i < SAMPLE_SIZE; i++) {
    if (!isnan(samples[i])) {
      sum += samples[i];
      sum_sq += samples[i] * samples[i];
      valid_count++;
    }
  }
  
  res.media = sum / valid_count;
  res.variancia = (sum_sq - valid_count * pow(res.media, 2)) / (valid_count - 1);
  res.desvio_padrao = sqrt(res.variancia);

  float sorted[SAMPLE_SIZE];
  memcpy(sorted, samples, SAMPLE_SIZE * sizeof(float));
  bubbleSort(sorted, SAMPLE_SIZE);
  
  res.q1 = sorted[SAMPLE_SIZE / 4];
  res.mediana = sorted[SAMPLE_SIZE / 2];
  res.q3 = sorted[3 * SAMPLE_SIZE / 4];
  res.iqr = res.q3 - res.q1;

  float lower_bound = res.q1 - IQR_FACTOR * res.iqr;
  float upper_bound = res.q3 + IQR_FACTOR * res.iqr;
  res.is_outlier = (new_val < lower_bound) || (new_val > upper_bound);

  return res;
}

/**
 * @brief Calcula correlação de Pearson entre duas variáveis
 * @param x Valores da primeira variável
 * @param y Valores da segunda variável
 * @param n Número de amostras
 * @return Coeficiente de correlação (-1 a 1)
 */
float calcularPearson(float x[], float y[], byte n) {
  float sum_x = 0, sum_y = 0, sum_xy = 0;
  float sum_x2 = 0, sum_y2 = 0;
  
  for (byte i = 0; i < n; i++) {
    sum_x += x[i];
    sum_y += y[i];
    sum_xy += x[i] * y[i];
    sum_x2 += x[i] * x[i];
    sum_y2 += y[i] * y[i];
  }
  
  float numerador = n * sum_xy - sum_x * sum_y;
  float denominador = sqrt((n * sum_x2 - sum_x * sum_x) * 
                     (n * sum_y2 - sum_y * sum_y));
  
  return (denominador != 0) ? numerador / denominador : 0;
}

// ================= FUNÇÕES DE TEMPO E DATA =================

/**
 * @brief Calcula data relativa a partir do tempo de execução
 * @param milliseconds Tempo decorrido em ms
 * @return String com data no formato YYYY-MM-DD
 */
String calcularDataRelativa(unsigned long milliseconds) {
  unsigned long totalSeconds = milliseconds / 1000;
  
  // Calcula o tempo decorrido desde o horário de início
  unsigned long secondsSinceStart = totalSeconds;
  
  // Verifica se passou da meia-noite no primeiro dia
  unsigned long initialDayOffset = 0;
  unsigned long initialDaySeconds = (START_HOUR * 3600UL) + (START_MINUTE * 60UL);
  if (secondsSinceStart < (86400UL - initialDaySeconds)) {
    // Ainda no primeiro dia
    secondsSinceStart = 0;
  } else {
    // Passou para o(s) próximo(s) dia(s)
    secondsSinceStart -= (86400UL - initialDaySeconds);
  }
  
  unsigned long daysToAdd = secondsSinceStart / 86400UL;
  
  int year = REFERENCE_YEAR;
  int month = REFERENCE_MONTH;
  int day = REFERENCE_DAY + daysToAdd;
  
  // Ajuste simplificado para meses com 30 dias
  while (day > 30) {
    month++;
    day -= 30;
    if (month > 12) {
      year++;
      month = 1;
    }
  }
  
  char dateBuffer[11];
  snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", year, month, day);
  return String(dateBuffer);
}

/**
 * @brief Formata o tempo decorrido em HH:MM:SS a partir do horário de início
 * @param milliseconds Tempo decorrido em ms
 * @return String com horário formatado
 */
String formatarTempo(unsigned long milliseconds) {
  unsigned long totalSeconds = milliseconds / 1000;
  unsigned long hours = (totalSeconds / 3600) % 24;
  unsigned long minutes = (totalSeconds / 60) % 60;
  unsigned long seconds = totalSeconds % 60;
  
  // Adiciona o horário de início
  hours = (START_HOUR + hours) % 24;
  minutes = (START_MINUTE + minutes);
  
  // Ajusta os minutos se ultrapassarem 60
  if (minutes >= 60) {
    hours = (hours + 1) % 24;
    minutes -= 60;
  }
  
  char timeBuffer[9];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(timeBuffer);
}

// ================= FUNÇÕES PRINCIPAIS =================

/**
 * @brief Atualiza buffers de correlação e calcula quando cheios
 * @param temp Temperatura atual
 * @param umid_ar Umidade do ar atual
 * @param umid_solo Umidade do solo atual
 * @return Array com 3 pares de correlações
 */
ResultadoCorrelacao* atualizarCorrelacao(float temp, float umid_ar, float umid_solo) {
  static ResultadoCorrelacao resultados[3] = {
    {0, false, "Temp", "UAr"},
    {0, false, "Temp", "USolo"},
    {0, false, "UAr", "USolo"}
  };
  
  temp_buffer_corr[corr_index] = temp;
  umid_ar_buffer_corr[corr_index] = umid_ar;
  umid_solo_buffer_corr[corr_index] = umid_solo;
  corr_index = (corr_index + 1) % CORR_BUFFER_SIZE;
  
  if (corr_index == 0) {
    resultados[0].coeficiente = calcularPearson(temp_buffer_corr, umid_ar_buffer_corr, CORR_BUFFER_SIZE);
    resultados[0].significativa = fabs(resultados[0].coeficiente) > CORR_THRESHOLD;
    
    resultados[1].coeficiente = calcularPearson(temp_buffer_corr, umid_solo_buffer_corr, CORR_BUFFER_SIZE);
    resultados[1].significativa = fabs(resultados[1].coeficiente) > CORR_THRESHOLD;
    
    resultados[2].coeficiente = calcularPearson(umid_ar_buffer_corr, umid_solo_buffer_corr, CORR_BUFFER_SIZE);
    resultados[2].significativa = fabs(resultados[2].coeficiente) > CORR_THRESHOLD;
  }
  
  return resultados;
}

/**
 * @brief Grava dados formatados no cartão SD
 * @param dados Leituras dos sensores
 * @param stats_umid_ar Estatísticas umidade ar
 * @param stats_temp Estatísticas temperatura
 * @param stats_umid_solo Estatísticas umidade solo
 * @param correlacoes Resultados das correlações
 * @param tempoDecorrido Tempo total desde inicialização
 */
void logData(DadosSensores dados, Estatisticas stats_umid_ar, 
            Estatisticas stats_temp, Estatisticas stats_umid_solo,
            ResultadoCorrelacao* correlacoes, unsigned long tempoDecorrido) {
  dataFile = SD.open("dados.csv", FILE_WRITE);
  
  if (dataFile) {
    if (dataFile.size() == 0) {
      dataFile.println(F("Data,Hora,UmidadeAr,MedUAr,DesvUAr,Q1UAr,MedUAr,Q3UAr,OutUAr,Temp,MedT,DesvT,Q1T,MedT,Q3T,OutT,USolo,MedUS,DesvUS,Q1US,MedUS,Q3US,OutUS,Corr1,Corr2,Corr3"));
      dataFile.println(F("# Data calculada a partir de 2025-04-18"));
      dataFile.println(F("# Hora calculada a partir de 18:46"));
    }

    // Data e hora relativas
    dataFile.print(calcularDataRelativa(tempoDecorrido));
    dataFile.print(',');
    dataFile.print(formatarTempo(tempoDecorrido));
    dataFile.print(',');
    
    // Dados umidade ar
    dataFile.print(dados.umidade_ar); dataFile.print(',');
    dataFile.print(stats_umid_ar.media); dataFile.print(',');
    dataFile.print(stats_umid_ar.desvio_padrao); dataFile.print(',');
    dataFile.print(stats_umid_ar.q1); dataFile.print(',');
    dataFile.print(stats_umid_ar.mediana); dataFile.print(',');
    dataFile.print(stats_umid_ar.q3); dataFile.print(',');
    dataFile.print(stats_umid_ar.is_outlier ? F("SIM") : F("NAO")); dataFile.print(',');
    
    // Dados temperatura
    dataFile.print(dados.temperatura); dataFile.print(',');
    dataFile.print(stats_temp.media); dataFile.print(',');
    dataFile.print(stats_temp.desvio_padrao); dataFile.print(',');
    dataFile.print(stats_temp.q1); dataFile.print(',');
    dataFile.print(stats_temp.mediana); dataFile.print(',');
    dataFile.print(stats_temp.q3); dataFile.print(',');
    dataFile.print(stats_temp.is_outlier ? F("SIM") : F("NAO")); dataFile.print(',');
    
    // Dados umidade solo
    dataFile.print(dados.umidade_solo_percent); dataFile.print(',');
    dataFile.print(stats_umid_solo.media); dataFile.print(',');
    dataFile.print(stats_umid_solo.desvio_padrao); dataFile.print(',');
    dataFile.print(stats_umid_solo.q1); dataFile.print(',');
    dataFile.print(stats_umid_solo.mediana); dataFile.print(',');
    dataFile.print(stats_umid_solo.q3); dataFile.print(',');
    dataFile.print(stats_umid_solo.is_outlier ? F("SIM") : F("NAO")); dataFile.print(',');
    
    // Correlações
    for (byte i = 0; i < 3; i++) {
      dataFile.print(correlacoes[i].coeficiente, 4);
      if (i < 2) dataFile.print(',');
    }
    
    dataFile.println();
    dataFile.close();
  }
}

/**
 * @brief Lê todos os sensores e retorna valores processados
 * @return Estrutura DadosSensores com leituras atuais
 */
DadosSensores lerSensores() {
  DadosSensores dados;
  dados.umidade_ar = dht.readHumidity();
  dados.temperatura = dht.readTemperature();
  int raw_value = analogRead(SOIL_PIN);
  dados.umidade_solo_percent = mapSoilMoistureToPercent(raw_value);
  return dados;
}

// ================= SETUP E LOOP PRINCIPAIS =================

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(SOIL_PIN, INPUT);
  
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("Erro no SD!"));
    while(1);
  }
  
  Serial.println(F("Sistema iniciado"));
  Serial.print(F("Data de referência: "));
  Serial.print(REFERENCE_YEAR); Serial.print("-");
  Serial.print(REFERENCE_MONTH); Serial.print("-");
  Serial.println(REFERENCE_DAY);
  Serial.print(F("Horário de início: "));
  Serial.print(START_HOUR); Serial.print(":");
  Serial.println(START_MINUTE);
}

void loop() {
  static unsigned long last_log = 0;
  
  if (millis() - last_log >= LOG_INTERVAL) {
    last_log = millis();
    
    DadosSensores dados = lerSensores();

    if (!isnan(dados.umidade_ar) && !isnan(dados.temperatura)) {
      // Atualiza buffers de amostras
      temp_samples[sample_index] = dados.temperatura;
      umid_ar_samples[sample_index] = dados.umidade_ar;
      umid_solo_samples[sample_index] = dados.umidade_solo_percent;
      sample_index = (sample_index + 1) % SAMPLE_SIZE;

      // Atualiza correlações
      ResultadoCorrelacao* correlacoes = atualizarCorrelacao(
        dados.temperatura, dados.umidade_ar, dados.umidade_solo_percent);
      
      // Calcula estatísticas
      Estatisticas stats_temp = calcularEstatisticas(temp_samples, dados.temperatura);
      Estatisticas stats_umid_ar = calcularEstatisticas(umid_ar_samples, dados.umidade_ar);
      Estatisticas stats_umid_solo = calcularEstatisticas(umid_solo_samples, dados.umidade_solo_percent);

      // Verifica outliers
      if (stats_temp.is_outlier) Serial.println(F("ALERTA: Outlier de temperatura!"));
      if (stats_umid_ar.is_outlier) Serial.println(F("ALERTA: Outlier de umidade do ar!"));
      if (stats_umid_solo.is_outlier) Serial.println(F("ALERTA: Outlier de umidade do solo!"));

      // Grava dados
      logData(dados, stats_umid_ar, stats_temp, stats_umid_solo, correlacoes, millis());
      
      // Exibe informações no serial para debug
      Serial.print(F("Registro gravado - "));
      Serial.print(calcularDataRelativa(millis()));
      Serial.print(" ");
      Serial.println(formatarTempo(millis()));
    }
  }
}
