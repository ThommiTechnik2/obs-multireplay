/*
obs-multireplay — unit tests for the responsive layout engine.
SPDX-License-Identifier: GPL-2.0-or-later

Standalone: no OBS, no Qt, no screen, so it runs in CI on every platform. That
is the point — the layout DECISION must be argued about here, not by opening
OBS and resizing a dock.

The properties pinned here are the ones the old system got wrong:

  - a group travels together (the wrap never splits it);
  - a hard line break survives the flow;
  - a flow's floor is its WIDEST ITEM, computable without knowing the
    arrangement (this is what kills the height-for-width circularity);
  - collapse is by priority, is monotone as width shrinks, and never takes a
    non-collapsible item;
  - the split picks side-by-side on a wide area and stacked on a tall one, and
    says so when neither fits instead of pretending the choice was free.
*/

#include "responsive-layout.hpp"

#include <cstdio>
#include <vector>

using namespace multireplay::ui;

static int g_fail = 0;

#define CHECK(cond)                                                         \
	do {                                                                \
		if (!(cond)) {                                              \
			std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, \
				    #cond);                                 \
			++g_fail;                                           \
		}                                                           \
	} while (0)

static FlowItem fi(int id, int w, Priority p = Priority::Primary, int group = 0)
{
	FlowItem it;
	it.id = id;
	it.minW = w;
	it.prefW = w;
	it.minH = 26;
	it.prio = p;
	it.group = group;
	return it;
}

static bool rowsAre(const std::vector<std::vector<int>> &rows,
		    const std::vector<std::vector<int>> &want)
{
	return rows == want;
}

static bool contains(const std::vector<int> &v, int id)
{
	for (int x : v)
		if (x == id)
			return true;
	return false;
}

int main()
{
	// ── greedy wrap ────────────────────────────────────────────────────
	{
		std::vector<FlowItem> its;
		for (int i = 1; i <= 5; ++i)
			its.push_back(fi(i, 100));
		auto rows = wrapFlow(its, 250, 0);
		CHECK(rowsAre(rows, {{1, 2}, {3, 4}, {5}}));
	}

	// ── hard line break survives ───────────────────────────────────────
	{
		std::vector<FlowItem> its = {fi(1, 10), fi(2, 10)};
		FlowItem b = fi(3, 10);
		b.breakBefore = true;
		its.push_back(b);
		auto rows = wrapFlow(its, 1000, 0);
		CHECK(rowsAre(rows, {{1, 2}, {3}}));
	}

	// ── a group travels together ───────────────────────────────────────
	{
		std::vector<FlowItem> its = {fi(1, 100), fi(2, 100, Priority::Primary, 7),
					     fi(3, 100, Priority::Primary, 7),
					     fi(4, 100, Priority::Primary, 7), fi(5, 100)};
		auto rows = wrapFlow(its, 250, 0);
		// group of three is wider than the row: it ships whole, not split.
		CHECK(rowsAre(rows, {{1}, {2, 3, 4}, {5}}));
	}

	// ── the floor is the widest item, width-independent ────────────────
	{
		std::vector<FlowItem> its = {fi(1, 40), fi(2, 50), fi(3, 60)};
		CHECK(flowMinWidth(its) == 60);
		CHECK(flowMinWidth(its) == flowMinWidth(its));
	}

	// ── collapse by priority ───────────────────────────────────────────
	{
		std::vector<FlowItem> its = {fi(1, 100, Priority::Critical),
					     fi(2, 100, Priority::Primary),
					     fi(3, 100, Priority::Secondary),
					     fi(4, 100, Priority::Tertiary)};
		FlowOptions o;
		o.gapX = 0;
		o.rowH = 26;
		o.gapY = 0;
		o.maxRows = 2;
		FlowPlan p = planFlow(its, 100, o);
		CHECK(p.rowCount == 2);
		CHECK(p.overflowed);
		// Tertiary then Secondary go; Critical and Primary stay.
		CHECK(contains(p.collapsed, 4));
		CHECK(contains(p.collapsed, 3));
		CHECK(!contains(p.collapsed, 1));
		CHECK(!contains(p.collapsed, 2));
	}

	// ── a non-collapsible item never collapses ─────────────────────────
	{
		std::vector<FlowItem> its = {fi(1, 100, Priority::Critical),
					     fi(2, 100, Priority::Tertiary)};
		its[0].collapsible = false;
		FlowOptions o;
		o.gapX = 0;
		o.maxRows = 1;
		FlowPlan p = planFlow(its, 100, o);
		CHECK(contains(p.collapsed, 2));
		CHECK(!contains(p.collapsed, 1));
		CHECK(p.rowCount == 1);
	}

	// ── monotone: narrower never means fewer rows ──────────────────────
	{
		std::vector<FlowItem> its;
		for (int i = 1; i <= 12; ++i)
			its.push_back(fi(i, 90));
		FlowOptions o;
		o.gapX = 6;
		int prev = 0;
		bool monotone = true;
		for (int w = 1200; w >= 90; w -= 37) {
			int r = flowNaturalRows(its, w, o.gapX);
			if (r < prev)
				monotone = false;
			prev = r;
		}
		CHECK(monotone);
	}

	// ── the honest floor is small whatever the width ───────────────────
	{
		std::vector<FlowItem> its = {fi(1, 60, Priority::Critical),
					     fi(2, 60, Priority::Secondary),
					     fi(3, 60, Priority::Tertiary)};
		FlowOptions o;
		o.rowH = 26;
		o.gapY = 4;
		CHECK(flowMinHeight(its, o) == 26); // one critical row
	}

	// ── determinism ────────────────────────────────────────────────────
	{
		std::vector<FlowItem> its;
		for (int i = 1; i <= 9; ++i)
			its.push_back(fi(i, 70, Priority::Primary));
		FlowOptions o;
		o.maxRows = 3;
		FlowPlan a = planFlow(its, 300, o);
		FlowPlan b = planFlow(its, 300, o);
		CHECK(a.rows == b.rows);
		CHECK(a.collapsed == b.collapsed);
	}

	// ── bands: drop the least useful first ─────────────────────────────
	{
		std::vector<BandItem> bands = {{1, 100, 100, Priority::Critical, true},
					       {2, 100, 100, Priority::Primary, true},
					       {3, 100, 100, Priority::Tertiary, true}};
		BandPlan p = planBands(bands, 150, 0);
		CHECK(contains(p.visible, 1));
		CHECK(contains(p.collapsed, 2));
		CHECK(contains(p.collapsed, 3));
		CHECK(p.usedH == 100);
	}

	// ── a fill band is never collapsed ─────────────────────────────────
	{
		std::vector<BandItem> bands = {{1, 100, 100, Priority::Critical, true},
					       {2, 50, 50, Priority::Tertiary, true}};
		bands[1].fill = true;
		BandPlan p = planBands(bands, 100, 0);
		CHECK(contains(p.visible, 2));
	}

	// ── split: wide ⇒ side, tall ⇒ stacked ─────────────────────────────
	{
		Extent a, b;
		a.minW = 300;
		a.minH = 200;
		b.minW = 300;
		b.minH = 200;
		CHECK(planSplit(a, b, 800, 600).axis == SplitAxis::Horizontal);
		CHECK(planSplit(a, b, 500, 900).axis == SplitAxis::Vertical);
	}

	// ── split says so when nothing fits ────────────────────────────────
	{
		Extent a, b;
		a.minW = 400;
		a.minH = 200;
		b.minW = 400;
		b.minH = 200;
		SplitPlan p = planSplit(a, b, 500, 300);
		CHECK(!p.fits);
	}

	if (g_fail == 0) {
		std::printf("responsive_layout: OK\n");
		return 0;
	}
	std::printf("responsive_layout: %d FAILED\n", g_fail);
	return 1;
}
