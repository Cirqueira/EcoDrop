# 📝 Desenvolvido — EcoDrop

Documento de registro das melhorias implementadas no sistema **EcoDrop** (dashboard web + automação de irrigação) a partir das diretrizes em `Documentacao/implementacao.md`.

---

## 1. Visão geral

O dashboard, que antes era aberto diretamente no `public/index.html`, agora é uma aplicação de página única (SPA) com:

- Tela de **Login** exibida ao carregar o sistema.
- **Cadastro exclusivo do administrador**: o cadastro público foi removido; apenas o usuário mestre vê o botão **Cadastrar usuário** e pode habilitar novas contas (e-mail + senha).
- **Usuário mestre** pré-provisionado.
- **Logout** com modal de confirmação e opção de **troca de senha**.
- Mensagem **"Bem vindo, (nome do usuário logado)"**.
- Ícone **Layout** com 4 skins/temas selecionáveis.
- Toda a funcionalidade original do dashboard preservada (cards, histórico, controle automático/manual da bomba).

Arquivo principal alterado: `public/index.html`.
Documentação gerada: `desenvolvido.md` (este arquivo).

---

## 2. Tecnologias utilizadas

| Recurso | Uso |
| :--- | :--- |
| **HTML5 + CSS3** | Interface, temas via variáveis CSS, modais e responsividade. |
| **JavaScript (vanilla)** | Lógica de autenticação, temas e painel em tempo real. |
| **Firebase Authentication** | Login, cadastro, logout e troca de senha (e-mail/senha). |
| **Firebase Realtime Database** | Fonte de dados dos sensores e comandos da bomba (mantido). |
| **Firebase Hosting** | Publicação da pasta `public/`. |

SDKs adicionados no `index.html` (modo *compat*):

```html
<script src="https://www.gstatic.com/firebasejs/10.8.0/firebase-app-compat.js"></script>
<script src="https://www.gstatic.com/firebasejs/10.8.0/firebase-auth-compat.js"></script>
<script src="https://www.gstatic.com/firebasejs/10.8.0/firebase-database-compat.js"></script>
```

---

## 3. Autenticação

### 3.1 Tela de Login

- Exibida ao carregar o sistema.
- Solicita **e-mail** e **senha**.
- Ao autenticar com sucesso, o sistema redireciona (na mesma página) para o **Dashboard**.
- Erros são traduzidos para português (ex.: senha incorreta, e-mail inválido, usuário não encontrado).

### 3.2 Cadastro de usuário (exclusivo do administrador)

- Não existe mais autocadastro na tela de login: apenas o **administrador** pode habilitar novos usuários.
- Ao entrar como `administrador@ecodrop.com`, o dashboard exibe o botão **Cadastrar usuário** no cabeçalho.
- O botão abre um modal que solicita **e-mail**, **senha** e **confirmação de senha**.
- Validações: senha mínima de 6 caracteres e confirmação igual.
- A criação usa uma **instância secundária do Firebase** (`ecodrop-admin`), evitando que a sessão do administrador seja encerrada/trocada ao criar a conta.
- O nome exibido na saudação dos novos usuários é derivado do e-mail (parte antes do `@`) e salvo em `displayName`.
- Usuários comuns **não visualizam** o botão de cadastro; a função também valida no envio que o usuário logado é o administrador.

### 3.3 Usuário administrador

O administrador é criado manualmente no **Firebase Console** (Authentication > Users), com e-mail e senha definidos pelo responsável do projeto. O e-mail reconhecido como administrador fica na constante `ADMIN_EMAIL` de `public/index.html` (padrão: `administrador@ecodrop.com`).

Por segurança, **nenhuma senha fica no código-fonte**. O provisionamento automático do usuário mestre foi removido do dashboard.
4. Marca em `localStorage` (`ecodrop_mestre_ok`) que o provisionamento já ocorreu.

> Caso a conta já exista (ex.: criada em outro navegador), o erro `auth/email-already-in-use` é tratado e o fluxo segue normalmente.

### 3.4 Saudação

Após o login, o cabeçalho exibe:

```
Bem vindo, <nome do usuário logado>
```

O nome prioriza `displayName`; se ausente, é derivado do e-mail.

### 3.5 Logout

O botão **Sair** abre um modal de confirmação com três ações:

- **Sair da conta** — encerra a sessão e volta para a tela de Login.
- **Trocar senha** — abre o formulário de alteração de senha.
- **Cancelar** — fecha o modal.

### 3.6 Troca de senha

- Solicita **senha atual**, **nova senha** e **confirmação**.
- Reautentica o usuário (`reauthenticateWithCredential`) antes de chamar `updatePassword`, evitando o erro `auth/requires-recent-login`.
- Exibe feedback por *toast* em caso de sucesso e mensagens de erro no próprio modal.

---

## 4. Layout / Skins

O ícone **Layout** no cabeçalho (ícone de grade + texto) abre um modal para escolher entre **4 temas**:

| Tema | Descrição |
| :--- | :--- |
| **Default** | Configuração visual original (verde claro e natural). |
| **Dark** | Fundo escuro com acentos verdes, para ambientes de baixa luz. |
| **Floresta** | Tons terrosos e verdes profundos. |
| **Oceano** | Paleta azul/teal, mais clara e refrescante. |

Detalhes da implementação:

- Temas implementados com **variáveis CSS** (`--bg`, `--text`, `--card-bg`, `--accent`, etc.) e o atributo `data-theme` no `<body>`.
- A preferência é salva em `localStorage` (`ecodrop_tema`) e reaplicada ao recarregar a página.
- O tema ativo é destacado no seletor.
- O modal fecha ao clicar fora dele, no **X** ou pressionando **Esc**.

---

## 5. Melhorias de usabilidade e segurança

- **Boot screen** com spinner até a resolução do estado de autenticação (evita piscar o login de um usuário já autenticado).
- **Modais** acessíveis por teclado/`Esc` e fechamento por clique no backdrop.
- **Feedback visual** com toasts (login, cadastro, tema, senha, logout).
- **Estados de carregamento** nos botões (evita envios duplicados).
- **Listeners do Realtime Database** são registrados ao entrar e removidos ao sair (`off`), evitando duplicidade e vazamento.
- Erros de autenticação traduzidos para o usuário final.
- Remoção de cores fixas no HTML em favor de variáveis, permitindo que todos os temas afetem cards, tabela, botões e status.

---

## 6. Funcionalidades originais preservadas

Nada foi removido do painel original:

- Cards de **Temperatura do Ar**, **Umidade do Solo** e **status da Bomba**.
- Tabela de histórico em tempo real (até 8 registros, com animação na linha nova).
- Alternância entre modo **Automático** e **Manual**.
- Acionamento manual da bomba (bloqueado quando o modo automático está ativo).
- Integração com os caminhos do Firebase já utilizados pelo firmware ESP32:
  - `dispositivos/ESP32_Solo_01/estado_atual`
  - `atuadores/bomba_dagua/modo`

O firmware (`src/transmissor_sensor` e `src/receptor_atuador`) **não foi alterado**, mantendo compatibilidade total com os dados existentes.

---

## 7. Como executar / publicar

### 7.1 Pré-requisitos no Firebase

1. No **Firebase Console**, acesse **Authentication → Sign-in method**.
2. Habilite o provedor **E-mail/senha**.
3. Em **Authentication → Settings → Authorized domains**, garanta que o domínio de hospedagem (e `localhost`, se for testar localmente) esteja autorizado.
4. Mantenha o **Realtime Database** com acesso compatível com o firmware atual (o ESP32 grava sem autenticação).

### 7.2 Execução local

```bash
firebase serve --only hosting
```

ou qualquer servidor estático apontando para a pasta `public/`.

### 7.3 Publicação

```bash
firebase deploy --only hosting
```

### 7.4 Primeiro acesso

1. No **Firebase Console**, cadastre o usuário administrador em Authentication > Users.
2. Confirme que o e-mail cadastrado é o mesmo definido em `ADMIN_EMAIL` no `public/index.html`.
3. Abra o sistema e faça login com a conta do administrador.
4. No dashboard, use o botão **Cadastrar usuário** para habilitar as contas dos demais usuários.
5. Saia e entre com a conta criada para validar o acesso ao dashboard.

---

## 8. Observações e recomendações

- O provisionamento do usuário mestre é feito pelo cliente (front-end) para atender ao requisito de existir uma conta padrão. Em um ambiente de produção real, recomenda-se criar o mestre via **Firebase Admin SDK** ou Console e **não** manter credenciais no código.
- Como a senha do mestre é definida no código, caso ela seja alterada pela tela de troca de senha, o valor padrão deixa de valer. Nesse caso, redefina a senha pelo Console do Firebase.
- Recomenda-se, em produção, evoluir as regras do Realtime Database para exigir autenticação no acesso web, mantendo um mecanismo seguro para o ESP32.
- A restrição de cadastro é feita na interface e na validação da função, suficiente para o uso previsto. Como o app é estático, um usuário malicioso ainda poderia chamar a API do Firebase Auth diretamente; para bloqueio real, use **Cloud Functions** (bloqueio de criação de conta), **App Check** ou desabilite a criação pública no Identity Platform.
- Para endurecer a segurança, pode-se adicionar verificação de e-mail, recuperação de senha e política de complexidade de senha.

---

## 9. Resumo dos arquivos

| Arquivo | Situação | Descrição |
| :--- | :--- | :--- |
| `public/index.html` | Alterado | Login, cadastro exclusivo do administrador, logout, troca de senha, saudação, temas e dashboard. |
| `desenvolvido.md` | Novo | Este documento. |
| `firebase.json` | Alterado | Headers `Cache-Control: no-cache` para evitar servir HTML desatualizado. |
| `src/**` | Inalterado | Firmware ESP32 preservado. |

---

Desenvolvido por Leonard Cirqueira — 2026
