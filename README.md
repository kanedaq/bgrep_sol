# bgrep version 1.5 について

- 機能 ： バイナリファイル中からパターンを検索します。
- 配布形態 ： Visual Studio 2019ソリューション、Windows用実行ファイル（32bit）
- 動作確認OS ： Windowsコマンドプロンプト / Ubuntu

# ファイル説明

- bgrep.exe ： Windowsコマンドプロンプト用の実行ファイル（32bit）
- bgrep.cpp ： C++ソースコード

# ビルド例（boostを使用しています）

- Visual C++ コマンド
```
    cl bgrep.cpp /EHsc
```

- g++
```
    g++ -o bgrep bgrep.cpp -std=c++0x -static -lboost_program_options -lboost_system
```

# プログラム説明

WindowsコマンドプロンプトやLinux端末等で、引数を与えずにコマンドを叩くと、以下のように使い方と使用例が表示されます。

```
Usage: bgrep [-swhlv] pattern file[s]
Options:
  -s [ --string ]       string search <default>
  -w [ --wstring ]      wstring search
  -h [ --hex ]          hex search
  -l [ --list ]         list match files
  -v [ --verbose ]      display messages
Usage examples:
    (Un*x) find . -type f -name '*' -print0 | xargs -0 bgrep pattern
    (Windows) dir /b /a-d /s * | xargs bgrep pattern
```

## -sオプション

与えられたパターンを、そのまま文字列として検索します（＝デフォルト）。

空白を含む文字列を検索するときは、例えば Windowsコマンドプロンプトではパターンをダブルクォーテーションで囲って、"Hello world" のように与えます。

## -wオプション

パターン文字列をワイド文字列に変換して検索します。

## -hオプション

パターンを16進文字列と解釈して検索します。

16進文字列は、例えば Windowsコマンドプロンプトではダブルクォーテーションで囲って、"61 CC 62 EB 63 90" のように空白区切りで与えます。

# 使用例

引数を与えずにコマンドを叩くと、使用例（Usage examples）も表示されます。

この使用例は、ファイル名をワイルドカードで指定して、かつサブディレクトリのファイルも検索する例です。
Un*xの例は、findコマンドと xargsコマンドを、bgrepと組み合わせています。

Windowsの例は、findコマンドの代わりに dirを使用しています。

ただし、Windowsには xargsコマンドがないので、「Windows xargs」でググってWindows用の xargsを拾ってくるか、あるいは Cygwinを導入して Un*xコマンドを使用してください。

# 未対応

2Gバイト超のファイルからの検索には対応していません。
