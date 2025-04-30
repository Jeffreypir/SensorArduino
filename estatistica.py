#!/usr/bin/python3

#==================== PROGRAM ==============================
# Program: estatistica
# Date of Create: 29/04/2025
# Update in: 29/04/2025
# Author:Jefferson Bezerra dos Santos
# Description: Gerar Estátisticas Percentuais
#===========================================================

import pandas as pd

# Lê o CSV
df = pd.read_csv("DADOS.CSV")

# Converte para numérico, ignorando erros
df_numeric = df.apply(pd.to_numeric, errors='coerce')

# Variáveis de interesse
variaveis = ['Temp', 'UmidadeAr', 'USolo']

# Calcula estatísticas relativas (% da média)
estatisticas_percentuais = {}

for var in variaveis:
    media = df_numeric[var].mean()
    min_val = df_numeric[var].min()
    max_val = df_numeric[var].max()
    intervalo = max_val - min_val
    desvio = df_numeric[var].std()
    variancia = df_numeric[var].var()
    mediana = df_numeric[var].median()
    
    estatisticas_percentuais[var] = {
        'Intervalo (%)': (intervalo / media) * 100 if media else None,
        'Desvio Padrão (%)': (desvio / media) * 100 if media else None,
        'Variância (%)': (variancia / media) * 100 if media else None,
        'Mediana (valor)': mediana,
        'Mediana (%)': (mediana / media) * 100 if media else None,
    }

# Cria DataFrame e exibe
df_resultado = pd.DataFrame(estatisticas_percentuais).T.round(2)

print("Estatísticas globais (% em relação à média):")
print(df_resultado)

# Salvar em CSV, se quiser
df_resultado.to_csv("estatisticas_percentuais.csv")


