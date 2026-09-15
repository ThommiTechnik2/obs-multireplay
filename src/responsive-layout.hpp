/*
obs-multireplay — the responsive layout engine
Copyright (C) 2026 obs-multireplay contributors
SPDX-License-Identifier: GPL-2.0-or-later

ONE engine, no arrangements. This header answers three questions the old panel
answered with three hand-authored layouts (Wide/Short/Tall) and a pile of
special cases:

  planFlow   how a row of controls wraps, and what has to move to "… More"
  planBands  which vertical sections are visible before the rest collapse
  planSplit  pictures beside the list, or above it

Everything here is pure C++20 — no Qt, no OBS — so it is unit-tested in
tests/standalone on every platform (see responsive_layout_test.cpp). That is
deliberate: the layout *decision* is the thing that must not be argued about at
runtime, and the only way to test "75% width collapses the trim keys, not the
transport" cheaply is to make the decision testable without a screen.

The one property that kills the old circular constraint: a flow's minimum width
is the WIDEST SINGLE ITEM (a flow can always wrap to one item per row), and its
minimum height is the COLLAPSED height — both computable without knowing the
final arrangement. The old code derived the floor from a height-for-width that
its own preference contradicted, which is why it needed a one-pass-late sample
(`wideFloorH_`) and a widget item that lied about its floor.
*/

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace multireplay {
namespace ui {

// ---------------------------------------------------------------------------
// Priorities. Lower value = survives longer. When space runs out the engine
// takes items away from the HIGHEST value first (Tertiary before Critical), so
// "Critical" means "this never leaves the panel while anything else is still
// on it".
// ---------------------------------------------------------------------------
enum class Priority : int {
	Critical = 0,
	Primary = 1,
	Secondary = 2,
	Tertiary = 3,
};

// ---------------------------------------------------------------------------
// A row item. `group` binds consecutive items that must travel together: the
// wrap never puts two members of one group on different rows unless the group
// alone is wider than the whole row (a group that cannot fit still ships as one
// row that overflows, because splitting it would be worse than a wide row).
//
// `breakBefore` is a hard line break — the caller's way of saying "start a new
// row here", which is how the toolbar's declared rows survive the flow.
// ---------------------------------------------------------------------------
struct FlowItem {
	int id = 0;
	int group = 0;
	int minW = 0;
	int prefW = 0;
	int minH = 0;
	Priority prio = Priority::Primary;
	bool collapsible = true;
	bool breakBefore = false;
};

struct FlowOptions {
	int gapX = 6;
	int gapY = 4;
	// Height of one row. The panel keeps ONE height for all keys (26 px normal,
	// 32 gallery), so a row is that height, not the max of mixed key sizes.
	int rowH = 26;
	// Maximum number of rows to show before collapsing; 0 = place a row for
	// every item that needs one (no height budget).
	int maxRows = 0;
};

struct FlowPlan {
	// ids per row, in order. Wrapping is greedy and group-aware.
	std::vector<std::vector<int>> rows;
	// ids that did not fit, in their ORIGINAL order (not the removal order).
	std::vector<int> collapsed;
	int rowCount = 0;
	// widest row, and total height including the vertical gaps.
	int width = 0;
	int height = 0;
	// true when something was moved to `collapsed`.
	bool overflowed = false;
};

// ---------------------------------------------------------------------------
// Vertical bands. This is the Tall dock (a stack of sections that is a wall of
// keys before the list) and the Short dock (where height is the scarce axis)
// expressed as one rule: give each band a priority and drop the least useful
// first. A band that is not collapsible stays even if the stack then overflows.
// ---------------------------------------------------------------------------
struct BandItem {
	int id = 0;
	int minH = 0;
	int prefH = 0;
	Priority prio = Priority::Primary;
	bool collapsible = true;
	// A fill band absorbs whatever height is left once the others are placed.
	bool fill = false;
};

struct BandPlan {
	std::vector<int> visible;
	std::vector<int> collapsed;
	// height the visible bands need, including gaps.
	int usedH = 0;
	bool overflowed = false;
};

// ---------------------------------------------------------------------------
// The split. `bias` lets the caller say which axis it prefers when both fit
// (gallery/maximised pulls toward side-by-side; a low dock pulls toward
// stacked). `fits` is false when neither axis fits the minimums, i.e. the
// panes are going to overflow whichever way they are put — the caller logs it
// rather than pretending the choice was free.
// ---------------------------------------------------------------------------
struct Extent {
	int minW = 0;
	int prefW = 0;
	int minH = 0;
	int prefH = 0;
};

enum class SplitAxis { Horizontal, Vertical };

struct SplitPlan {
	SplitAxis axis = SplitAxis::Vertical;
	bool fits = true;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// The width an item asks for while packing. A caller that only knows the floor
// (minW) gets the floor.
inline int flowItemWidth(const FlowItem &it)
{
	return std::max(it.minW, it.prefW);
}

// A flow can always wrap to one item per row, so its floor is its widest item.
// This is width-independent BY CONSTRUCTION — it is what lets the widget
// answer minimumWidth() honestly.
inline int flowMinWidth(const std::vector<FlowItem> &items)
{
	int w = 0;
	for (const auto &it : items)
		w = std::max(w, std::max(it.minW, it.prefW));
	return w;
}

inline int flowHeightForRows(int rows, int rowH, int gapY)
{
	if (rows <= 0)
		return 0;
	return rows * rowH + (rows - 1) * gapY;
}

// Greedy, group-aware wrap. Kept public because the gate and the test both need
// "which rows" without the collapse decision.
inline std::vector<std::vector<int>> wrapFlow(const std::vector<FlowItem> &items,
					      int availW, int gapX)
{
	std::vector<std::vector<int>> rows;
	std::vector<int> cur;
	int curW = 0;

	// A cluster is a run of consecutive items sharing a non-zero group, or a
	// single item when group == 0.
	std::size_t i = 0;
	while (i < items.size()) {
		std::size_t j = i + 1;
		if (items[i].group != 0) {
			while (j < items.size() &&
			       items[j].group == items[i].group)
				++j;
		}
		int clusterW = 0;
		for (std::size_t k = i; k < j; ++k) {
			if (k > i)
				clusterW += gapX;
			clusterW += flowItemWidth(items[k]);
		}
		const bool brk = items[i].breakBefore;
		const int lead = cur.empty() ? 0 : gapX;
		if (!cur.empty() && (brk || curW + lead + clusterW > availW)) {
			rows.push_back(cur);
			cur.clear();
			curW = 0;
		}
		for (std::size_t k = i; k < j; ++k) {
			if (!cur.empty())
				curW += gapX;
			cur.push_back(items[k].id);
			curW += flowItemWidth(items[k]);
		}
		i = j;
	}
	if (!cur.empty())
		rows.push_back(cur);
	return rows;
}

inline int flowNaturalRows(const std::vector<FlowItem> &items, int availW, int gapX)
{
	return (int)wrapFlow(items, availW, gapX).size();
}

namespace detail {

// Removal candidates, most disposable first: highest priority value first,
// and within a priority the LATER item (the older, less-used control). Only
// collapsible items are candidates.
inline std::vector<std::size_t> collapseOrder(const std::vector<FlowItem> &items)
{
	std::vector<std::size_t> idx;
	for (std::size_t k = 0; k < items.size(); ++k)
		if (items[k].collapsible)
			idx.push_back(k);
	std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
		if (items[a].prio != items[b].prio)
			return (int)items[a].prio > (int)items[b].prio;
		return a > b;
	});
	return idx;
}

} // namespace detail

// Drop least-important items until the wrap fits `maxRows`. A group loses its
// members one at a time; the remaining members stay grouped. Returns the plan.
inline FlowPlan planFlow(const std::vector<FlowItem> &items, int availW,
			 const FlowOptions &opt)
{
	FlowPlan plan;

	std::vector<char> gone(items.size(), 0);
	auto live = [&]() {
		std::vector<FlowItem> v;
		for (std::size_t k = 0; k < items.size(); ++k)
			if (!gone[k])
				v.push_back(items[k]);
		return v;
	};

	auto rows = wrapFlow(live(), availW, opt.gapX);
	const std::size_t limit =
		opt.maxRows > 0 ? (std::size_t)opt.maxRows : (std::size_t)-1;

	if (opt.maxRows > 0 && rows.size() > limit) {
		const auto order = detail::collapseOrder(items);
		for (std::size_t c : order) {
			if (rows.size() <= limit)
				break;
			gone[c] = 1;
			rows = wrapFlow(live(), availW, opt.gapX);
		}
	}

	plan.rows = rows;
	plan.rowCount = (int)rows.size();
	plan.height = flowHeightForRows(plan.rowCount, opt.rowH, opt.gapY);
	for (const auto &r : rows) {
		int w = 0;
		for (std::size_t k = 0; k < r.size(); ++k) {
			if (k)
				w += opt.gapX;
			for (const auto &it : items)
				if (it.id == r[k]) {
					w += flowItemWidth(it);
					break;
				}
		}
		plan.width = std::max(plan.width, w);
	}
	for (std::size_t k = 0; k < items.size(); ++k)
		if (gone[k])
			plan.collapsed.push_back(items[k].id);
	plan.overflowed = !plan.collapsed.empty();
	return plan;
}

// The honest floor, and the reason this engine exists. At the floor the panel
// shows only what it refuses to lose; everything else is one click away in
// "… More". Independent of the arrangement the panel happens to be in.
inline int flowMinHeight(const std::vector<FlowItem> &items, const FlowOptions &opt,
			 int floorRows = 1)
{
	int rows = 0;
	for (const auto &it : items)
		if (!it.collapsible || it.prio == Priority::Critical)
			++rows;
	// A critical group is one row, not one per member.
	int criticalRows = 0;
	bool prevCritical = false;
	for (const auto &it : items) {
		const bool crit = !it.collapsible || it.prio == Priority::Critical;
		if (crit && !prevCritical)
			++criticalRows;
		prevCritical = crit;
	}
	(void)rows;
	return flowHeightForRows(std::max(criticalRows, floorRows), opt.rowH, opt.gapY);
}

// ---------------------------------------------------------------------------
// Bands
// ---------------------------------------------------------------------------
inline int bandsTotalH(const std::vector<BandItem> &items, int gapY)
{
	int h = 0;
	for (const auto &b : items)
		h += std::max(b.minH, b.prefH);
	if (!items.empty())
		h += (int)(items.size() - 1) * gapY;
	return h;
}

inline BandPlan planBands(const std::vector<BandItem> &items, int availH,
			  int gapY = 4)
{
	BandPlan plan;
	std::vector<char> gone(items.size(), 0);

	auto visibleH = [&]() {
		int h = 0, n = 0;
		for (std::size_t k = 0; k < items.size(); ++k) {
			if (gone[k])
				continue;
			h += std::max(items[k].minH, items[k].prefH);
			++n;
		}
		if (n > 1)
			h += (n - 1) * gapY;
		return h;
	};

	if (availH > 0) {
		std::vector<std::size_t> order;
		for (std::size_t k = 0; k < items.size(); ++k)
			if (items[k].collapsible && !items[k].fill)
				order.push_back(k);
		std::stable_sort(order.begin(), order.end(),
				 [&](std::size_t a, std::size_t b) {
					 if (items[a].prio != items[b].prio)
						 return (int)items[a].prio >
							(int)items[b].prio;
					 return a > b;
				 });
		for (std::size_t c : order) {
			if (visibleH() <= availH)
				break;
			gone[c] = 1;
		}
	}

	for (std::size_t k = 0; k < items.size(); ++k) {
		if (gone[k])
			plan.collapsed.push_back(items[k].id);
		else
			plan.visible.push_back(items[k].id);
	}
	plan.usedH = 0;
	int n = 0;
	for (std::size_t k = 0; k < items.size(); ++k) {
		if (gone[k])
			continue;
		plan.usedH += std::max(items[k].minH, items[k].prefH);
		++n;
	}
	if (n > 1)
		plan.usedH += (n - 1) * gapY;
	plan.overflowed = !plan.collapsed.empty();
	return plan;
}

// ---------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------
inline SplitPlan planSplit(const Extent &a, const Extent &b, int availW, int availH,
			   double bias = 0.0)
{
	const bool sideFits = a.minW + b.minW <= availW &&
			      std::max(a.minH, b.minH) <= availH;
	const bool stackFits = std::max(a.minW, b.minW) <= availW &&
			       a.minH + b.minH <= availH;

	SplitPlan plan;
	if (sideFits && stackFits)
		plan.axis = (double)availW >= (double)availH * (1.0 + bias)
				    ? SplitAxis::Horizontal
				    : SplitAxis::Vertical;
	else if (sideFits)
		plan.axis = SplitAxis::Horizontal;
	else if (stackFits)
		plan.axis = SplitAxis::Vertical;
	else {
		plan.axis = (double)availW >= (double)availH * (1.0 + bias)
				    ? SplitAxis::Horizontal
				    : SplitAxis::Vertical;
		plan.fits = false;
	}
	plan.fits = plan.fits && (sideFits || stackFits);
	return plan;
}

} // namespace ui
} // namespace multireplay
