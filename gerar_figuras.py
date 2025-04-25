import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.decomposition import PCA
from sklearn.preprocessing import StandardScaler
import os

# Configuração inicial
try:
    plt.style.use('seaborn-v0_8')
except:
    plt.style.use('ggplot')

sns.set_theme(style="whitegrid")
plt.rcParams['font.size'] = 12

# Carregar dados
try:
    dados = pd.read_csv('DADOS.CSV')
    dados['Data_Hora'] = pd.to_datetime(dados['Data'] + ' ' + dados['Hora'], dayfirst=False)
except Exception as e:
    print(f"Erro ao carregar dados: {str(e)}")
    exit()

# Verificação das colunas necessárias
required_columns = ['UmidadeAr', 'Temp', 'USolo']
for col in required_columns:
    if col not in dados.columns:
        print(f"Erro: Coluna '{col}' não encontrada no arquivo CSV.")
        exit()

# 1. Gráficos Temporais Individuais
def plot_graficos_temporais(df):
    try:
        # Configuração
        variaveis = {
            'UmidadeAr': {'nome': 'Umidade do Ar', 'cor': 'blue', 'unidade': '%'},
            'Temp': {'nome': 'Temperatura', 'cor': 'red', 'unidade': '°C'},
            'USolo': {'nome': 'Umidade do Solo', 'cor': 'green', 'unidade': '%'}
        }
        
        # Gráfico combinado
        fig, axs = plt.subplots(3, 1, figsize=(12, 15))
        
        for i, (col, info) in enumerate(variaveis.items()):
            # Gráfico individual no subplot
            axs[i].plot(df['Data_Hora'], df[col], color=f'tab:{info["cor"]}')
            axs[i].set_title(f'Variação Temporal da {info["nome"]}')
            axs[i].set_ylabel(f'{info["nome"]} ({info["unidade"]})')
            axs[i].grid(True)
            
            # Gráfico individual separado
            plt.figure(figsize=(12, 4))
            plt.plot(df['Data_Hora'], df[col], color=f'tab:{info["cor"]}')
            plt.title(f'Variação Temporal da {info["nome"]}')
            plt.ylabel(f'{info["nome"]} ({info["unidade"]})')
            plt.xlabel('Horário')
            plt.grid(True)
            plt.tight_layout()
            plt.savefig(f'graficos/temporal_{col.lower()}.png', dpi=300, bbox_inches='tight')
            plt.close()
        
        axs[-1].set_xlabel('Horário')
        plt.tight_layout()
        plt.savefig('graficos/temporal_combinado.png', dpi=300, bbox_inches='tight')
        plt.close()
        
    except Exception as e:
        print(f"Erro ao gerar gráficos temporais: {str(e)}")

# 2. Boxplot das Variáveis
def plot_boxplot(df):
    try:
        plt.figure(figsize=(10, 6))
        df_melt = df.melt(value_vars=['UmidadeAr', 'Temp', 'USolo'], 
                         var_name='Variável', value_name='Valor')
        
        sns.boxplot(x='Variável', y='Valor', data=df_melt)
        plt.title('Distribuição das Variáveis Ambientais')
        plt.tight_layout()
        plt.savefig('graficos/boxplot_variaveis.png', dpi=300, bbox_inches='tight')
        plt.close()
    except Exception as e:
        print(f"Erro ao gerar boxplot: {str(e)}")

# 3. Matriz de Correlação
def plot_correlacao(df):
    try:
        corr = df[['UmidadeAr', 'Temp', 'USolo']].corr()
        
        plt.figure(figsize=(8, 6))
        sns.heatmap(corr, annot=True, cmap='coolwarm', center=0, 
                    annot_kws={'size': 14}, fmt='.2f')
        plt.title('Matriz de Correlação')
        plt.tight_layout()
        plt.savefig('graficos/matriz_correlacao.png', dpi=300, bbox_inches='tight')
        plt.close()
    except Exception as e:
        print(f"Erro ao gerar matriz de correlação: {str(e)}")

# 4. Análise de Outliers (Umidade)
def plot_outliers(df):
    try:
        plt.figure(figsize=(12, 6))
        
        # Cálculo de limites para outliers (método IQR)
        Q1 = df['UmidadeAr'].quantile(0.25)
        Q3 = df['UmidadeAr'].quantile(0.75)
        IQR = Q3 - Q1
        limite_inf = Q1 - 1.5*IQR
        limite_sup = Q3 + 1.5*IQR
        
        # Plot com outliers destacados
        ax = sns.scatterplot(x='Data_Hora', y='UmidadeAr', data=df, 
                            hue=df['UmidadeAr'].apply(
                                lambda x: 'Outlier' if x < limite_inf or x > limite_sup else 'Normal'),
                            palette={'Normal':'blue', 'Outlier':'red'},
                            s=80)
        
        # Linhas de limite
        plt.axhline(y=limite_sup, color='r', linestyle='--')
        plt.axhline(y=limite_inf, color='r', linestyle='--')
        
        plt.title('Identificação de Outliers - Umidade do Ar')
        plt.xticks(rotation=45)
        plt.legend(title='Classificação')
        plt.tight_layout()
        plt.savefig('graficos/outliers_umidade.png', dpi=300, bbox_inches='tight')
        plt.close()
    except Exception as e:
        print(f"Erro ao gerar gráfico de outliers: {str(e)}")

# 5. Análise PCA
def plot_pca(df):
    try:
        # Pré-processamento
        X = df[['UmidadeAr', 'Temp', 'USolo']].values
        X = StandardScaler().fit_transform(X)
        
        # PCA
        pca = PCA(n_components=2)
        principalComponents = pca.fit_transform(X)
        
        # Plot
        plt.figure(figsize=(10, 6))
        plt.scatter(principalComponents[:, 0], principalComponents[:, 1], alpha=0.5)
        
        # Vetores de características
        for i, feature in enumerate(['Umidade', 'Temp', 'USolo']):
            plt.arrow(0, 0, pca.components_[0, i], pca.components_[1, i],
                     color='r', width=0.01, head_width=0.05)
            plt.text(pca.components_[0, i]*1.2, pca.components_[1, i]*1.2, feature, color='r')
        
        plt.xlabel(f'PC1 (Variancia: {pca.explained_variance_ratio_[0]:.1%})')
        plt.ylabel(f'PC2 (Variancia: {pca.explained_variance_ratio_[1]:.1%})')
        plt.title('Análise de Componentes Principais (PCA)')
        plt.grid()
        plt.tight_layout()
        plt.savefig('graficos/pca_ambiental.png', dpi=300, bbox_inches='tight')
        plt.close()
    except Exception as e:
        print(f"Erro ao gerar PCA: {str(e)}")

# Criar diretório para gráficos
os.makedirs('graficos', exist_ok=True)

# Gerar todos os gráficos
plot_graficos_temporais(dados)
plot_boxplot(dados)
plot_correlacao(dados)
plot_outliers(dados)
plot_pca(dados)

print("Processamento concluído. Verifique a pasta 'graficos/'")
print("Arquivos gerados:")
print("- temporal_combinado.png (gráfico com 3 subplots)")
print("- temporal_umidadear.png (umidade do ar individual)")
print("- temporal_temp.png (temperatura individual)")
print("- temporal_usolo.png (umidade do solo individual)")
print("- boxplot_variaveis.png")
print("- matriz_correlacao.png")
print("- outliers_umidade.png")
print("- pca_ambiental.png")
