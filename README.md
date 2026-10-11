# ImGui Menu

Interface desktop em **C++17**, utilizando **Dear ImGui**, **Win32** e **DirectX 11**.

> Projeto de demonstração de interface. Os controles apresentados não constituem integração funcional com jogos. As páginas sem implementação devem ser exibidas como indisponíveis.

## Prévia da interface

![Prévia da interface ImGui](docs/ui-preview.png)

> A imagem acima representa a aparência visual de referência. O arquivo `docs/ui-preview.png` precisa estar presente no repositório para aparecer no GitHub.

## Requisitos

- Windows 10 ou 11 (x64).
- Visual Studio 2022 com **Desktop development with C++**.
- MSVC v143 e Windows SDK 10.0 ou superior.
- DirectX 11 (bibliotecas do Windows SDK).
- C++17.

O código do Dear ImGui e os backends Win32/DX11 estão incluídos em `ImGuiMenu/third_party/imgui/`. Consulte `LICENSE.txt` nessa pasta para a licença de terceiros.

## Como compilar

1. Clone o repositório e abra `ImGuiMenu/ImGuiMenu.sln` no Visual Studio 2022.
2. Selecione a plataforma **x64**.
3. Selecione **Debug** ou **Release**.
4. Use **Build > Build Solution** (`Ctrl+Shift+B`).
5. Execute o binário gerado em `ImGuiMenu/bin/x64/<Configuração>/ImGuiMenu.exe`.

Pelo **Developer Command Prompt for VS 2022**, também é possível executar:

```bat
msbuild ImGuiMenu\ImGuiMenu.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild ImGuiMenu\ImGuiMenu.sln /m /p:Configuration=Release /p:Platform=x64
```

## Estrutura atual

```text
ImGuiMenu/
├── ImGuiMenu.sln
├── ImGuiMenu.vcxproj
├── src/
│   ├── main.cpp      # Janela Win32 e renderização DirectX 11
│   └── menu.h        # Interface, componentes e estado
└── third_party/
    └── imgui/        # Dear ImGui e backends Win32/DX11
docs/
└── ui-preview.png    # Captura da interface
```

## Interface

A navegação apresenta as seções Aim, Visuals, Players, Misc e Config. A página Aim reúne controles de demonstração como switches, sliders, seleção segmentada, combos, cores e atalhos de teclado. A busca filtra nomes das seções da barra lateral na branch de refatoração. As demais páginas ainda não estão implementadas e devem mostrar um aviso de indisponibilidade.

**Atenção:** os valores visuais da demonstração não executam ações em jogos; não confunda controles de interface com funcionalidades externas implementadas.

## Limitações e testes

- A arquitetura ainda está concentrada em `menu.h`; a modularização está pendente.
- Persistência de configurações ainda não foi implementada.
- O layout contém medidas fixas e ainda precisa de melhorias de DPI e responsividade.
- O comportamento dos controles personalizados precisa de validação adicional.
- **Build Debug x64:** não executado neste ambiente.
- **Build Release x64:** não executado neste ambiente.
- **Testes manuais de interface:** não executados neste ambiente.

## Solução de problemas

- **MSB8020 (v143):** instale as ferramentas MSVC v143 pelo Visual Studio Installer.
- **SDK não encontrado:** instale o Windows 10/11 SDK e ajuste o projeto se necessário.
- **Erro de include ImGui:** confirme que `third_party/imgui` e `third_party/imgui/backends` estão presentes.
- **Janela não abre:** confira o suporte a DirectX 11, atualize drivers e consulte a saída de depuração do Visual Studio.
- **Imagem não aparece no README:** confira se `docs/ui-preview.png` foi incluído no commit.

## Licenças

Consulte `LICENSE` na raiz e `ImGuiMenu/third_party/imgui/LICENSE.txt`.
