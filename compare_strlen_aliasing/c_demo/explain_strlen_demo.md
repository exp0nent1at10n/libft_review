written by claude code

# c_demo の説明

「word 単位で読む strlen が strict aliasing で壊れる現象」を、C だけで再現するデモです。背景(strict aliasing、`__may_alias__`、word 単位のゼロ判定の式)と出典は [compare_strlen.md](../python/compare_strlen.md) の 0〜2 章にあるので、ここでは繰り返しません。

## 1. ビルドと実行

同じソースを、オプションだけ変えてビルドし、出力を見比べます。

```sh
make        # 下の 4 つを順に「ビルド → 実行」して、出力をまとめて表示
make O2     # 1 つだけ(O0 / O2 / O3 / nsa)
make fclean # 生成物を削除
```

バイナリは `strlen_demo` の 1 つだけで、オプションを変えるたびに作り直します。`make O2` などは、次のコマンドを実行するのと同じです。

| ターゲット | コマンド |
|---|---|
| `make O0` | `gcc -O0 main.c strlen_no_optimize.c -o strlen_demo && ./strlen_demo` |
| `make O2` | `gcc -O2 main.c strlen_no_optimize.c -o strlen_demo && ./strlen_demo` |
| `make O3` | `gcc -O3 main.c strlen_no_optimize.c -o strlen_demo && ./strlen_demo` |
| `make nsa` | `gcc -O2 -fno-strict-aliasing main.c strlen_no_optimize.c -o strlen_demo && ./strlen_demo` |

(実際には `-Wall -Wextra` も付く。各出力の前に `== -O2` のような見出しが出る)

GCC 専用です(`noipa`、`always_inline`、`__may_alias__`、`__builtin_assume_aligned` を使う)。

## 2. 期待する結果

`-O2` / `-O3` で、`uint32_t` で書き込んだ行の `N=7` だけ、`ft_strlen_size_t` が 8 を返します。

```
[uint32_t written]
N=7: ft_strlen=7  ft_strlen_size_t=8  (may_alias=7)
N=8: ft_strlen=8  ft_strlen_size_t=8  (may_alias=8)
N=9: ft_strlen=9  ft_strlen_size_t=9  (may_alias=9)
```

それ以外はすべて N と同じ値になります。

| 条件 | 結果 | 理由 |
|---|---|---|
| `-O0` | 正しい | strict aliasing を使った最適化が有効にならない |
| `-O2 -fno-strict-aliasing` | 正しい | 型の違うストアとロードも、同じメモリを指しうるとみなす |
| `char` 書き込み | 正しい | `char` は何とでも alias できる |
| `uint64_t` 書き込み | 正しい | x86_64 では `uint64_t` と `size_t` が同じ型 |
| `may_alias` 版 | 正しい | `__may_alias__` 付きの型での読み出しは、`char` での読み出しと同じ扱い |
| `ft_strlen` | 正しい | `char` でしか読まない |

N=7 だけが誤り、N=8 と 9 が正しい理由は [compare_strlen.md](../python/compare_strlen.md) の 5.1 節と同じです。最初の 8 バイト(1 word)を古い内容で「ゼロ無し」と判定して 2 word 目へ進むため、ゼロが 1 word 目にある N=7 だけ結果が変わります。

未定義動作なので、「壊れうる」であって「必ず壊れる」ではありません。

`-flto` を付けても(`-O2 -flto` / `-O3 -flto`)、結果は同じでした。

## 3. ファイルと関数の構成

| ファイル | 中身 |
|---|---|
| [strlen_demo.h](strlen_demo.h) | 定数(`BUF_BYTES`、`POOL_BYTES`、`LEN_CNT`)と、`strlen_no_optimize_*` のプロトタイプ |
| [ft_strlens.h](ft_strlens.h) | strlen 3 種の定義(`static inline`)と `alias_size_t` |
| [strlen_no_optimize.c](strlen_no_optimize.c) | 書き込み用の関数と、`strlen_no_optimize_*`(書いて strlen で読む関数) |
| [main.c](main.c) | `pool` の準備、`strlen_no_optimize_*` の呼び出し、結果の表示 |

| 関数 | ファイル | 役割 |
|---|---|---|
| `ft_strlen` | ft_strlens.h | 1 バイトずつ数える([ft_strlen.c](../ft_strlen.c) と同じ) |
| `ft_strlen_size_t` | ft_strlens.h | `size_t` 単位で読む。may_alias なし([ft_strlen_sizet.c](../ft_strlen_sizet.c) と同じ) |
| `ft_strlen_may_alias` | ft_strlens.h | `ft_strlen_size_t` の `wide_s` の型だけを `alias_size_t`(`__may_alias__` 付きの `size_t`)に替えたもの |
| `make_string` | strlen_no_optimize.c | `unsigned char b[16]` に、N 文字の文字列(`"ABCDEFG..."`)+ ゼロを作る |
| `write_char` / `write_uint32` / `write_uint64` | strlen_no_optimize.c | `b[]` の内容を、それぞれの型のストアで `buf` に書く |
| `strlen_no_optimize_char` / `_uint32` / `_uint64` | strlen_no_optimize.c | 書き込み型ごとに 1 つ。N = 7, 8, 9 で「書く → strlen で読む」を繰り返し、結果を `out` に入れる |
| `reset_pool` | main.c | `pool` 全体を `'A'` で埋め、末尾 1 バイトだけ 0 にする |
| `print_result` | main.c | 結果を表示する |

`out[0]`、`out[1]`、`out[2]` は、それぞれ `ft_strlen`、`ft_strlen_size_t`、`ft_strlen_may_alias` の結果です。

## 4. 書き方の理由

壊れる条件は、次の 2 つが揃うことです。

1. strlen が、書き込みと同じ関数の中にインライン展開される。
2. 書き込みが、`size_t` と互換性のない型(`uint32_t`)のストアである。

このデモの書き方は、すべてこの条件を作るため、または余計な最適化で条件が崩れないようにするためのものです。

### 4.1 strlen 3 種と write_* に `static inline __attribute__((always_inline))`

条件 1 を満たすためです。`strlen_no_optimize_*` の中に strlen と書き込みが展開されて、初めてコンパイラが「`uint32_t` のストア」と「`size_t` のロード」を並べて見られます。

出典: [GCC Common Function Attributes] `always_inline`("For functions declared inline, this attribute inlines the function independent of any restrictions that otherwise apply to inlining.")

### 4.2 strlen 3 種をヘッダ(ft_strlens.h)に定義で置く

インライン展開するには、呼び出す側の翻訳単位(.c ファイル)から関数の本体が見えている必要があります。strlen 3 種を別の .c に置くと、`-flto` なしでは `strlen_no_optimize.c` から本体が見えず、展開されません(条件 1 が崩れる)。そのため、本体ごと `static inline` でヘッダに置いています。

同じ理由で、`make_string` と `write_*` は `strlen_no_optimize.c` の中に置いています。

### 4.3 `strlen_no_optimize_*` に `__attribute__((noipa))`、引数で `pool`・`lens`・個数を渡す

`noinline` ではなく `noipa` にしているのは、`-O3` で再現させるためです。

- 個数(3)や `lens` の中身がコンパイル時に分かると、`-O3` は N=7, 8, 9 のループを完全に展開します。ループが無くなると、「`size_t` のロードをループの外へ移す」最適化(壊れる原因)が起きません。
- `noinline` は関数を展開しないだけで、引数の定数を関数の中へ伝える最適化(IPA-CP)は止めません。
- `noipa` は、呼び出し側と関数本体を互いに見えないものとして扱うので、`cnt` や `lens` は関数の中では「中身の分からない値」になります。

`main.c` と `strlen_no_optimize.c` は別の翻訳単位なので、`-flto` なしならそもそも定数は伝わりません。`noipa` は、`-flto` を付けたとき(ファイルをまたいで最適化されるとき)にも同じ結果にするためのものです。

GCC 15.2.0 (x86_64) で、まだ 1 ファイルだった版で試した結果は次のとおりです(`ft_strlen_size_t` × `uint32_t` の N=7, 8, 9)。

| 書き方 | `-O2` | `-O3` |
|---|---|---|
| `noinline` + グローバル変数を直接使う | 8, 8, 9 | 7, 8, 9(壊れない) |
| `noinline` + 引数で渡す | 8, 8, 9 | 7, 8, 9(壊れない) |
| `noinline` + 引数で渡し、`volatile` な個数を渡す | 8, 8, 9 | 7, 8, 9(壊れない) |
| `noinline` + 関数ポインタ(`volatile`)経由で呼ぶ | 8, 8, 9 | 8, 8, 9 |
| `noipa` + グローバル変数を直接使う | 8, 8, 9 | 7, 8, 9(壊れない) |
| **`noipa` + 引数で渡す(今の形)** | 8, 8, 9 | 8, 8, 9 |

関数ポインタ経由でも再現しますが、`noipa` のほうが意図(最適化の壁)がコードから読み取れるので、こちらを採用しました。[compare_strlen.py](../python/compare_strlen.py) は関数ポインタの表で呼んでいるので、`-O3` でも再現していました。

出典: [GCC Common Function Attributes] `noipa`("It disables interprocedural optimizations between the function with this attribute and its callers, as if the body of the function is not available when optimizing callers and the callers are unavailable when optimizing the body. This attribute implies `noinline`, `noclone` and `no_icf` attributes.")、`noinline`("It also disables some other interprocedural optimizations; it's preferable to use the more comprehensive `noipa` attribute instead if that is your goal.")

### 4.4 strlen ごとにループを分ける

1 つのループの中で strlen 3 種を続けて呼ぶ形も試しました。GCC 15.2.0 では、次のように結果が崩れました。

- `-O2`: `may_alias` 版まで N=7 で 8 を返した(同じアドレスからのロードを、`ft_strlen_size_t` の値と共通化したと考えられる。ダンプでは未確認)。
- `-O3`: 何も壊れなかった。

そのため、各 strlen が独立して「書く → 読む」を繰り返すように、ループを 3 つに分けています。

### 4.5 `__builtin_assume_aligned(pool, 8)`

`buf` が 8 バイト境界だとコンパイラに教えるためです。境界が分からないと、strlen 先頭の「1 バイトずつ読んで境界まで進む」ループが残り、壊れ方が見えにくくなります([compare_strlen.md](../python/compare_strlen.md) の 2 章)。`pool` 自体も `aligned(64)` で確保しています。

### 4.6 `reset_pool` と `volatile`

`pool` を `'A'` で埋めておくのは、ロードがループの外へ移ったときに「ゼロ無し」の古い内容が読まれ、結果に表れるようにするためです。`volatile` で書くのは、この初期化を最適化で消させないためです。書き込み型ごとに前の結果が残らないよう、毎回埋め直しています。

### 4.7 `write_uint32` / `write_uint64` の `memcpy`

`b[]` の 4 バイト(8 バイト)を `memcpy` で 1 つの `uint32_t`(`uint64_t`)にまとめてから、`buf[i] = v;` でストアします。ここが「`uint32_t` で書く」にあたります。`memcpy` 自体は `char` 単位のコピーなので、ここで aliasing の問題は起きません。

compare_strlen.py はシフト(`v |= (uint32_t)b[4 * i + j] << (8 * j)`)でまとめていました。GCC 15.2.0 では、どちらの書き方でも結果は同じでした。

## 5. 壊れる仕組み

`gcc -O2 -fdump-tree-optimized=- -c -o /dev/null strlen_no_optimize.c` の出力のうち、`strlen_no_optimize_uint32` の 2 つ目のループ(`ft_strlen_size_t`)の部分です(GCC 15.2.0 での抜粋。右の `<-` は説明。一時変数の番号はコンパイラやソースの違いで変わる)。

```
<bb 8>:                                       <- 2 つ目のループに入る前(1 回だけ実行)
  _194 = BIT_FIELD_REF <vect_patt_227..., 64, 0>;  <- 1 つ目のループで最後に書いた値(N=9)の先頭 8 バイト
  _144 = _194 + 18374403900871474943;          <- x - 0x0101...01
  _145 = ~_194;
  _146 = _144 & _145;
  _147 = _146 & 9259542123273814144;           <- & 0x8080...80 (先頭 word のゼロ判定)

<bb 9>:                                       <- 2 つ目のループの先頭
  # ivtmp.216_195 = PHI <...>
  ...                                          <- N 文字の文字列を作って buf に書く
  MEM <unsigned char[16]> [...buf_23] = MEM <unsigned char[16]> [...&b];
  if (_147 == 0)                               <- ループの外で計算した判定を使っている
    goto <bb 10>;                              <- 2 word 目以降の word ループへ
```

先頭 word のゼロ判定(`_147`)が、ループに入る前に 1 回だけ計算されています。その元の値は、ループの中で今書いた文字列ではなく、1 つ目のループ(`ft_strlen`)が最後に書いた N=9 の文字列 `"ABCDEFGH"` です。そのため、N=7 でも「先頭 word にゼロ無し」と判定され、8 を返します。

compare_strlen.md の 5.2 節(GCC 10.5.0)では、古い内容は `'A'` で埋めた初期値でした。古い値の出どころは違いますが、「型の違うストアの後のロードを、ループの外へ移した」という点は同じです。

ほかに、ダンプから次のことも分かりました。

- 書き込み(`memcpy` + `buf[i] = v`)は、最終的に 16 バイトのまとめたコピー(`{ref-all}`、つまり `char` 扱い)に変わっていました。ロードをループの外へ移したのは、それより前の段階(まだ `uint32_t` のストアだった段階)だと考えられます。
- 1 つ目のループの `ft_strlen`(1 バイトずつ)は、`__builtin_strlen` の呼び出しに置き換えられていました。

## 6. デバッグするときの手順

### 6.1 まず確認すること

```sh
gcc --version          # GCC 8 以上か(noipa 属性のため)
uname -m               # x86_64 か(リトルエンディアン、size_t = uint64_t の前提)
```

- `cc` が clang を指している環境では、`gcc` と明示してください。clang は `noipa` を知らず、警告を出して無視すると考えられます(未確認。clang では結果が変わりうる)。

### 6.2 期待どおりに壊れないとき

上から順に試します。

1. **`-O2` と `-O3` の両方で試す。** どちらか片方だけで壊れることがあります。
2. **ダンプで、ロードがループの外へ移っているかを見る。**
   ```sh
   gcc -O2 -fdump-tree-optimized=- -c -o /dev/null strlen_no_optimize.c \
     | awk '/^;; Function strlen_no_optimize_uint32 /,/^}/' > dump_uint32.txt
   ```
   `ft_strlen_size_t` のループ(`MEM[(const size_t *)` が出てくる部分)の前に、`18374403900871474943`(`0xFEFE...FF`)と `9259542123273814144`(`0x8080...80`)を使った計算が、ループの外(そのループの最初の `PHI` より前)にあるかを確認します。ループの中にしか無ければ、ロードは移っておらず、壊れないのが正しい挙動です。
3. **アセンブリで見る。**
   ```sh
   gcc -O2 -S -o strlen_no_optimize_O2.s strlen_no_optimize.c
   ```
   `strlen_no_optimize_uint32` の中で、先頭 word の 8 バイトロード(`movq (%reg), %rax` など)がループの前にあるかを見ます。
4. **呼び出し方を compare_strlen.py の形に近づける。** 4.3 節の表のとおり、関数ポインタ経由でも再現しました。`noipa` で再現しない場合は、`main` から `volatile` な関数ポインタ経由で呼ぶ形を試します。
   ```c
   void (*volatile f)(char *, const int *, int, int[3][LEN_CNT]) = strlen_no_optimize_uint32;
   f(pool, lens, LEN_CNT, out);
   ```
5. **書き込みを compare_strlen.py と同じシフトにする。** 4.7 節の `memcpy` を、`v = 0; for (j = 0; j < 4; j++) v |= (uint32_t)b[4 * i + j] << (8 * j);` に替えます。
6. **compare_strlen.py を同じマシンで動かす。** `python3 compare_strlen.py` で、そのコンパイラで壊れる条件(72 条件)がまとめて分かります。そこで壊れていなければ、そのコンパイラではこの現象自体が起きないと判断できます。

### 6.3 逆に、壊れてほしくないところが壊れたとき

- **`may_alias` 版が壊れた:** 4.4 節のように、別の strlen のロードと共通化されていないかを、ダンプの `alias_size_t` が出てくる部分で確認します。
- **`-O0` や `-fno-strict-aliasing` で壊れた:** 想定外です。`make_string` か `write_*` の詰め方が間違っている可能性が高いので、`ft_strlen` の結果(常に正しいはず)から確認します。

## 7. 出典

- [GCC Common Function Attributes]: GCC マニュアル「Common Function Attributes」 https://gcc.gnu.org/onlinedocs/gcc/Common-Attributes.html (`noipa`、`noinline`、`always_inline` の引用はこのページの最新版から。2026-10-26 に確認)
- 背景(strict aliasing の規則、`__may_alias__`、`-fstrict-aliasing`、ゼロ判定の式)の出典は [compare_strlen.md](../python/compare_strlen.md) の「出典一覧」を参照。

## 8. 確認していないこと

- `noipa` が GCC 8 で追加されたこと(記憶によるもの。GCC 8 のリリースノートで未確認)。
- clang での挙動。
- 4.4 節の「`may_alias` 版まで壊れた」原因。
- GCC 15.2.0 以外での、このデモ(c_demo)の実行結果。
