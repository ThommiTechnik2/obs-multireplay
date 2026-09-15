#!/usr/bin/env bash
#
# obs-multireplay — every obs_module_text() key the code asks for must exist in
# every locale file.
#
# Why this is a script and not a habit: obs_module_text() returns THE KEY when
# it cannot find a translation, so a missing entry is not a blank label or a
# warning — it is the string "Dock.ZoneAngles" printed on a button, in every
# language at once. Three of them shipped that way, and the only way anybody
# would have noticed is by looking at that particular corner of the panel.
#
# The audit's own note on method: where an invariant matters, it needs a
# mechanical check in CI, not a sentence in a comment.

set -euo pipefail
cd "$(dirname "$0")/.."

# TWO WAYS A KEY REACHES THE CODE, because one grep cannot see both:
#
#   1. obs_module_text("Dock.X") — direct, but it may be split across lines
#      (a call wrapped by the formatter), so the sources are joined first.
#   2. A helper that takes the key as an argument, e.g. section(page,
#      "Dock.SecStorage") in the settings dialog: the literal is there but the
#      call is not obs_module_text, so the first pass misses it. Three section
#      keys shipped that way before this second pattern was added.
keys="$(
	{
		cat src/*.cpp src/*.hpp | tr '\n' ' ' \
			| grep -oE 'obs_module_text\([[:space:]]*"[^"]+"' \
			| sed -E 's/^[^"]*"//; s/"$//'
		grep -ohE 'section\([A-Za-z_][A-Za-z0-9_]*[[:space:]]*,[[:space:]]*"[^"]+"' \
			src/*.cpp src/*.hpp \
			| sed -E 's/^[^"]*"//; s/"$//'
	} | sort -u
)"

if [ -z "${keys}" ]; then
	echo "check-locale-keys: found no keys at all — the grep is wrong" >&2
	exit 2
fi

fail=0
for ini in data/locale/*.ini; do
	have="$(grep -ohE '^[A-Za-z0-9._]+=' "${ini}" | sed 's/=$//' | sort -u)"
	missing="$(comm -23 <(echo "${keys}") <(echo "${have}") || true)"
	if [ -n "${missing}" ]; then
		echo "MISSING in ${ini}:" >&2
		echo "${missing}" | sed 's/^/  /' >&2
		fail=1
	fi
done

# The loop above proves that every key the code asks for exists somewhere; it
# says nothing about the locale files agreeing with each other. A key added to
# one .ini and not to the other is the same bug from the operator's seat: OBS
# prints the English, or the key itself where even that is missing. Compare the
# sorted key sets pairwise — two files today, but the nested loop grows with the
# set instead of needing a rewrite the day a third language lands.
locales=(data/locale/*.ini)
for ((i = 0; i < ${#locales[@]}; i++)); do
	for ((j = i + 1; j < ${#locales[@]}; j++)); do
		a="${locales[$i]}"
		b="${locales[$j]}"
		a_keys="$(grep -ohE '^[A-Za-z0-9._]+=' "${a}" | sed 's/=$//' | sort -u)"
		b_keys="$(grep -ohE '^[A-Za-z0-9._]+=' "${b}" | sed 's/=$//' | sort -u)"
		only_a="$(comm -23 <(echo "${a_keys}") <(echo "${b_keys}") || true)"
		only_b="$(comm -13 <(echo "${a_keys}") <(echo "${b_keys}") || true)"
		if [ -n "${only_a}" ]; then
			echo "ONLY in ${a} (missing from ${b}):" >&2
			echo "${only_a}" | sed 's/^/  /' >&2
			fail=1
		fi
		if [ -n "${only_b}" ]; then
			echo "ONLY in ${b} (missing from ${a}):" >&2
			echo "${only_b}" | sed 's/^/  /' >&2
			fail=1
		fi
	done
done

if [ "${fail}" -ne 0 ]; then
	echo "check-locale-keys: FAILED" >&2
	exit 1
fi

echo "check-locale-keys: $(echo "${keys}" | wc -l | tr -d ' ') key(s), all present in $(ls data/locale/*.ini | wc -l | tr -d ' ') locale file(s)"
