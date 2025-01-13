## Descrição do código

Este relatório se trata do arquivo **main.c**. Nesse arquivo, temos a implementação parcial da primeira entrega do trabalho. Ela não está completa pois eu não adquiri o hardware necessário para fazer os logs e o registro de data com exatidão, contudo, deixei o código pronto para receber esse hardware.

Irei falar nesse relatório sobre o código usando as linhas de código. As linhas referentes são as do github (pois existem outros editores de texto que modificam as linhas). [Projeto-log-com-rel-gio/main/main.c at main · Yurovskyy/Projeto-log-com-rel-gio · GitHub](https://github.com/Yurovskyy/Projeto-log-com-rel-gio/blob/main/main/main.c)

Das linhas 1 até a 21, temos os includes, que são as chamadas de bibliotecas externas para facilitar a aplicação do código.

Das linhas 23 a 46 temos constantes e variáveis do bluetooth.

Das linhas 49 a 76 temos as constantes e variáveis do gpio, responsáveis pelos botões físicos. A variável aguardando é uma flag para entrar no modo de troca de senha.

Das linhas 79 a 95 temos as funções para realizar a comunicação via bluetooth, tanto para enviar como para receber dados.

A função da linha 98 é o callback principal do bluetooth. A parte que nos interessa está na linha 143, que é quando chega dados via bluetooth. Dentro desse espaço está o código principal do arquivo.

A parte importante começa na linha 158, que verifica se recebemos uma mensagem.  Removemos os 2 ultimos caracteres hexadecimais na linha 161
A linha 162 faz uma verificação se estamos no modo aguardando (mudança de senha). Dentro dessa condição fazemos a mudança da senha.
o Else if da linha 184 ativa a flag aguardando.
O Else if da linha 190 é a parte onde digitamos a senha nos botões. Temos um for para preencher toda a senha, depois verificamos se ele acertou a senha (num if).
Depois, temos um if e else que printam a mensagem de sucesso ou falha. Nessa parte que será implementado o código faltante do hardware externo.

O restante do arquivo se trata de linhas de código de configuração e outros detalhes para que o bluetooth funcione corretamente (esse arquivo foi baseado no exemplo do bluetooth para evitar erros desnecessários).
