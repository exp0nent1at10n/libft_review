#!/usr/bin/env python3
"""ft_strlen 3実装の strict aliasing 耐性を比較する(このファイル単体で完結)。

比較対象:
  bytewise : ft_strlen.c       (1バイトずつ)
  sizet    : ft_strlen_sizet.c (size_t 単位で読む。may_alias なし)
  mayalias : sizet の size_t を __may_alias__ 付きの型に差し替えたもの

使い方: python3 compare_strlen.py        (コンパイラを変えるときは CC=clang など)
前提: Linux x86_64 (リトルエンディアン) + GCC。詳細は compare_strlen.md。
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# ---------------------------------------------------------------------------
# 比較対象の C ソース (ft_strlen*.c の関数本体をそのまま埋め込む)
# ---------------------------------------------------------------------------
SRC_BYTEWISE = r"""
size_t	ft_strlen(const char *s)
{
	size_t			count;

	count = 0;
	while (*s)
	{
		s++;
		count++;
	}
	return (count);
}
"""

SRC_SIZET = r"""
size_t	ft_strlen(const char *s)
{
	const char		*start;
	const size_t	*wide_s;
	size_t			ones;
	size_t			highs;

	start = s;
	while ((uintptr_t)s % (uintptr_t) sizeof(size_t))
	{
		if (!*s)
			return (s - start);
		s++;
	}
	ones = SIZE_MAX / 0xFF;
	highs = ones * 0x80;
	wide_s = (const size_t *)s;
	while (!((*wide_s - ones) & ~(*wide_s) & highs))
		wide_s++;
	s = (const char *)wide_s;
	while (*s)
		s++;
	return (s - start);
}
"""

# sizet の読み出し型だけを may_alias 付きに差し替える
SRC_MAYALIAS, _n = re.subn(
    r"const size_t(\s+)\*wide_s", r"const alias_size_t\1*wide_s", SRC_SIZET
)
assert _n == 1, "may_alias 版の生成に失敗(ft_strlen_sizet.c の宣言が変わった?)"

IMPLS = [("bytewise", SRC_BYTEWISE), ("sizet", SRC_SIZET), ("mayalias", SRC_MAYALIAS)]

# ---------------------------------------------------------------------------
# 条件
# ---------------------------------------------------------------------------
# 書き込み型: (名前, サイズ)。uint64_t は x86_64 Linux では unsigned long = size_t と同じ型
WRITERS = [("char", 1), ("uint32_t", 4), ("uint64_t", 8)]

# (名前, strlen の定義方法, 追加フラグ)
MODES = [
    ("inline", "static inline __attribute__((always_inline))", []),
    ("noinline", "static __attribute__((noinline))", []),
    ("sepTU+LTO", None, ["-flto"]),
]

FLAG_SETS = [
    [],  # 指定なし(gcc では -O0 相当)
    ["-O2"],
    ["-O3"],
    ["-O2", "-fno-strict-aliasing"],
]

# bytewise は char でしか読まないので壊れようがない。基準として1条件だけ実行する
BASELINE = ("inline", ["-O3"], "uint32_t")
LENGTHS = [7, 8, 9]  # 書き込む文字列の長さ(文字数)
BUF_BYTES = 16               # 文字列を書く領域(8バイト境界)
POOL_BYTES = 64              # バッファ全体。'A' で埋め、末尾だけ 0

COMMON_HEAD = """#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef size_t __attribute__((__may_alias__)) alias_size_t;
"""


def impl_defs(attr):
    return "\n".join(
        re.sub(r"size_t\s+ft_strlen\(", f"{attr} size_t len_{name}(", src, count=1)
        for name, src in IMPLS
    )


def test_fn(impl, wname, wsize):
    """lens[k] 文字の文字列を wname 型のストアで書き、strlen の結果を out[k] に入れる。"""
    return f"""
static __attribute__((noinline)) void test_{impl}_{wname}(char *pool, const int *lens, int cnt, int *out)
{{
	{wname}	*buf = __builtin_assume_aligned(pool, 8);
	unsigned char	b[{BUF_BYTES}];
	int				k;
	int				i;
	int				j;

	for (k = 0; k < cnt; k++)
	{{
		for (i = 0; i < {BUF_BYTES}; i++)
			b[i] = i < lens[k] ? (unsigned char)('A' + i % 26) : 0;
		for (i = 0; i < {BUF_BYTES // wsize}; i++)
		{{
			{wname} v = 0;
			for (j = 0; j < {wsize}; j++)
				v |= ({wname})b[{wsize} * i + j] << (8 * j);
			buf[i] = v;
		}}
		out[k] = (int)len_{impl}((const char *)buf);
	}}
}}"""


def build_sources(attr):
    """(呼び出し側 .c, strlen 側 .c or None, ケース一覧[(impl, writer)]) を返す。"""
    caller = [COMMON_HEAD]
    impl_tu = None
    if attr is None:  # 別ファイル
        impl_tu = COMMON_HEAD + impl_defs("")
        caller += [f"size_t len_{n}(const char *s);" for n, _ in IMPLS]
    else:
        caller.append(impl_defs(attr))
    cases = [(i, w) for i, _ in IMPLS for w, _ in WRITERS]
    sizes = dict(WRITERS)
    for impl, wname in cases:
        caller.append(test_fn(impl, wname, sizes[wname]))
    caller.append("\nstruct tc { void (*f)(char *, const int *, int, int *); };")
    caller.append("static const struct tc tcs[] = {")
    caller += [f"\t{{ test_{i}_{w} }}," for i, w in cases]
    caller.append("};")
    lens = ", ".join(map(str, LENGTHS))
    caller.append(f"""
static char			pool[{POOL_BYTES}] __attribute__((aligned(64)));
static const int	lens[] = {{ {lens} }};

int main(int argc, char **argv)
{{
	volatile int	idx;
	volatile char	*vp;
	int				out[{len(LENGTHS)}];
	int				i;

	if (argc < 2)
		return (2);
	vp = pool;
	for (i = 0; i < {POOL_BYTES - 1}; i++)
		vp[i] = 'A';
	vp[{POOL_BYTES - 1}] = 0;
	idx = atoi(argv[1]);
	tcs[idx].f(pool, lens, {len(LENGTHS)}, out);
	for (i = 0; i < {len(LENGTHS)}; i++)
		printf("%d ", out[i]);
	printf("\\n");
	return (0);
}}""")
    return "\n".join(caller), impl_tu, cases


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def flag_label(flags):
    return " ".join(flags) if flags else "(none)"


def run_case(exe, idx):
    """1ケース実行し、(表示文字列, 全部正しいか) を返す。"""
    try:
        r = run([str(exe), str(idx)], timeout=5)
    except subprocess.TimeoutExpired:
        return "HANG", False
    if r.returncode < 0:
        return "CRASH", False
    got = list(map(int, r.stdout.split()))
    cells = [f"{g}" + ("" if e == g else "*") for e, g in zip(LENGTHS, got)]
    return ", ".join(cells), got == LENGTHS


def show_evidence(cc, tmp, flags, caller_src, writer):
    """壊れた条件の GIMPLE から、ループに入る前の部分を sizet / mayalias で並べる。"""
    c_file = tmp / "ev.c"
    c_file.write_text(caller_src)
    r = run([cc, *flags, "-w", "-fdump-tree-optimized=-", "-c", "-o", "/dev/null", str(c_file)])
    print(f"\n--- 証拠: {flag_label(flags)} の GIMPLE(最適化後の中間表現), {writer} 書き込み, ループに入る前まで ---")
    for impl in ("sizet", "mayalias"):
        m = re.search(rf"^test_{impl}_{writer} \(.*?^}}", r.stdout, re.S | re.M)
        print(f"\n[{impl}]")
        if not m:
            print("(ダンプが見つからない)")
            continue
        body = m.group(0)
        body = body[body.index("<bb 2>"):]
        for ln in body.splitlines():
            if not ln.strip():
                continue
            note = ""
            if "(const size_t *)" in ln or "alias_size_t" in ln:
                note = "   <-- size_t 単位のロード"
            print(ln.rstrip() + note)
            if "PHI" in ln:  # 最初のループ先頭
                print("  ... (ここからループ)")
                break


def main():
    cc = os.environ.get("CC", "gcc")
    if shutil.which(cc) is None:
        sys.exit(f"{cc} が見つかりません")
    arch = run(["uname", "-m"]).stdout.strip()
    print(f"{run([cc, '--version']).stdout.splitlines()[0]} / {arch}")
    if arch != "x86_64":
        print("warning: x86_64 以外は想定外です(uint64_t が unsigned long とは限りません)")

    n_cases = len(IMPLS) * len(WRITERS)
    print(f"""
【検証内容】
  64バイトのバッファ全体を 'A' で埋めておき(末尾だけ 0)、先頭 16 バイトに
  「長さ N の文字列 + ゼロ終端」を書き込んでから strlen を呼ぶ。これを N = {LENGTHS} の順に繰り返す。
  書き込みは char / uint32_t / uint64_t のいずれかの型のストアで行う。""")

    tmp = Path(tempfile.mkdtemp(prefix="strlen_cmp_"))
    labels = [flag_label(f) for f in FLAG_SETS]
    try:
        n_wrong = 0
        baseline = None
        first_inline = None  # 証拠表示用: 最初に壊れた inline の条件
        sources = {}
        for mode, attr, extra in MODES:
            caller_src, impl_src, cases = build_sources(attr)
            sources[mode] = caller_src
            (tmp / "caller.c").write_text(caller_src)
            srcs = [str(tmp / "caller.c")]
            if impl_src:
                (tmp / "impl.c").write_text(impl_src)
                srcs.append(str(tmp / "impl.c"))
            rows = [i for i, (impl, _) in enumerate(cases) if impl != "bytewise"]
            table = {}  # (case index, flag index) -> 表示文字列
            for fi, flags in enumerate(FLAG_SETS):
                exe = tmp / "cmp"
                exe.unlink(missing_ok=True)
                r = run([cc, *flags, *extra, "-w", "-o", str(exe), *srcs])
                if r.returncode != 0:
                    sys.exit(f"compile failed ({mode}, {flags}):\n{r.stderr}")
                for i, (impl, writer) in enumerate(cases):
                    if impl == "bytewise":  # 基準は1条件だけ実行する
                        if (mode, flags, writer) == BASELINE:
                            baseline = run_case(exe, i)[0]
                        continue
                    text, ok = run_case(exe, i)
                    table[i, fi] = text
                    if not ok:
                        n_wrong += 1
                        if first_inline is None and mode == "inline" and impl == "sizet":
                            first_inline = (flags, writer)

            print(f"\n=== strlen: {mode} ===")
            w0 = 20
            col = max(len(", ".join(f"{n}*" for n in LENGTHS)), *(len(lb) for lb in labels)) + 2
            head = f"{'impl / writer':<{w0}}" + "".join(f"{lb:<{col}}" for lb in labels)
            print(head)
            print("-" * len(head))
            for i in rows:
                impl, writer = cases[i]
                print(f"{impl + ' / ' + writer:<{w0}}" + "".join(f"{table[i, fi]:<{col}}" for fi in range(len(FLAG_SETS))))

        total = len(MODES) * len(FLAG_SETS) * (len(IMPLS) - 1) * len(WRITERS)
        print(f"\n基準: 1バイトずつ読む bytewise (char 読みなので壊れようがない)を {BASELINE[0]}, "
              f"{flag_label(BASELINE[1])}, {BASELINE[2]} 書き込みで1回だけ実行 → {baseline}")
        print(f"誤った長さを返した条件: {n_wrong} / {total}   (* が付いたセルが誤り)")

        if first_inline:
            flags, writer = first_inline
            show_evidence(cc, tmp, flags, sources["inline"], writer)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
