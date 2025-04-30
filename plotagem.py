#!/usr/bin/python3

#==================== PROGRAM ==============================
# Program: Plotagem
# Date of Create: 29/04/2025
# Update in: 29/04/2025
# Author:Jefferson Bezerra dos Santos
# Description: Programa para geração de gráficos auxiliares
#===========================================================

import pandas as pd
import matplotlib.pyplot as plt

# Lê o CSV
df = pd.read_csv("DADOS.CSV")

# Cria uma coluna de datetime combinando Data + Hora
df['DataHora'] = pd.to_datetime(df['Data'] + ' ' + df['Hora'])

# Converte valores numéricos (ignora 'SIM', 'NAO' etc.)
df_numeric = df.apply(pd.to_numeric, errors='coerce')

# Junta com o datetime
df_numeric['DataHora'] = df['DataHora']

# Define o eixo X
x = df_numeric['DataHora']

# Inicia o gráfico
plt.figure(figsize=(12, 6))

# Plota as variáveis
plt.plot(x, df_numeric['Temp'], label='Temperatura (°C)', color='red', marker='o')
plt.plot(x, df_numeric['UmidadeAr'], label='Umidade do Ar (%)', color='blue', marker='s')
plt.plot(x, df_numeric['USolo'], label='Umidade do Solo (%)', color='green', marker='^')

# Configurações do gráfico
plt.title('Evolução das Variáveis ao Longo do Tempo')
plt.xlabel('Data e Hora')
plt.ylabel('Valor')
plt.xticks(rotation=45)
plt.grid(True)
plt.legend()
plt.tight_layout()

# Exibe
plt.show()


