/*
obs-multireplay — responsive widgets (implementation)
Copyright (C) 2026 obs-multireplay contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "responsive-widgets.hpp"

#include <QBoxLayout>
#include <QResizeEvent>
#include <QSplitter>

#include <algorithm>

namespace multireplay {
namespace ui {

namespace {

// The three numbers the engine needs from a control, read at the moment it is
// added or refreshed — never cached across a text/mode change.
void measureControl(FlowItem &meta, QWidget *w)
{
	int minW = std::max(w->minimumSizeHint().width(), w->minimumWidth());
	int prefW = std::max(minW, w->sizeHint().width());
	// A control that declares a MAXIMUM (the bank strip's 300px cap, the
	// project selector's 280) is laid out at most that wide; the flow must
	// measure it the same way or it would hand it a cell the cap forbids.
	const int maxW = w->maximumWidth();
	if (maxW > 0 && maxW < 100000) {
		prefW = std::min(prefW, maxW);
		minW = std::min(minW, maxW);
	}
	meta.minW = minW;
	meta.prefW = std::max(minW, prefW);
	meta.minH = std::max(w->minimumSizeHint().height(), w->minimumHeight());
}

// A template so it can read ResponsiveFlow's private Cell without the type
// being named here (Cell's members are public).
template <class Cells>
std::vector<FlowItem> itemsOf(const Cells &cells)
{
	std::vector<FlowItem> items;
	for (const auto &c : cells)
		if (c.active)
			items.push_back(c.meta);
	return items;
}

} // namespace

Metrics metricsFor(bool gallery)
{
	Metrics m;
	m.keyH = gallery ? 32 : 26;
	m.keyFoldedH = gallery ? 28 : 22;
	m.headerH = 34;
	m.zoneGap = gallery ? 20 : 14;
	m.bandGap = 4;
	m.laneGapMax = gallery ? 220 : 140;
	m.captionH = 13;
	return m;
}

// ---------------------------------------------------------------------------
// ResponsiveFlow
// ---------------------------------------------------------------------------
ResponsiveFlow::ResponsiveFlow(QWidget *parent) : QWidget(parent)
{
	metrics_ = metricsFor(false);
	opts_.gapX = 6;
	opts_.gapY = metrics_.bandGap;
	opts_.rowH = metrics_.keyH;
	// No layout of our own, on purpose: a QLayout would impose its own
	// height-for-width floor and we want the engine to be the only authority.
}

void ResponsiveFlow::setMetrics(const Metrics &m)
{
	metrics_ = m;
	opts_.gapY = m.bandGap;
	opts_.rowH = m.keyH;
	relayout();
}

void ResponsiveFlow::addControl(QWidget *w, int group, Priority prio,
				bool collapsible, bool breakBefore)
{
	if (!w)
		return;
	w->setParent(this);
	Cell c;
	c.w = w;
	c.meta.id = (int)cells_.size() + 1;
	c.meta.group = group;
	c.meta.prio = prio;
	c.meta.collapsible = collapsible;
	c.meta.breakBefore = breakBefore;
	measureControl(c.meta, w);
	opts_.rowH = std::max(opts_.rowH, w->sizeHint().height());
	cells_.push_back(c);
	relayout();
}

void ResponsiveFlow::clearControls()
{
	cells_.clear();
	plan_ = FlowPlan{};
	updateGeometry();
}

void ResponsiveFlow::setControlActive(QWidget *w, bool active)
{
	for (Cell &c : cells_)
		if (c.w == w) {
			if (c.active == active)
				return;
			c.active = active;
			relayout();
			return;
		}
}

bool ResponsiveFlow::controlActive(const QWidget *w) const
{
	for (const Cell &c : cells_)
		if (c.w == w)
			return c.active;
	return false;
}

void ResponsiveFlow::refresh()
{
	for (Cell &c : cells_) {
		if (!c.active)
			continue;
		measureControl(c.meta, c.w);
		opts_.rowH = std::max(opts_.rowH, c.w->sizeHint().height());
	}
	relayout();
}

void ResponsiveFlow::setMoreButton(QWidget *b)
{
	more_ = b;
	if (more_)
		more_->setParent(this);
	relayout();
}

int ResponsiveFlow::naturalRowsForWidth(int w) const
{
	const std::vector<FlowItem> items = itemsOf(cells_);
	return flowNaturalRows(items, std::max(1, w), opts_.gapX);
}

int ResponsiveFlow::naturalHeightForWidth(int w) const
{
	const std::vector<FlowItem> items = itemsOf(cells_);
	const int rows = flowNaturalRows(items, std::max(1, w), opts_.gapX);
	return flowHeightForRows(rows, opts_.rowH, opts_.gapY);
}

void ResponsiveFlow::relayout()
{
	applyPlan();
}

void ResponsiveFlow::applyPlan()
{
	const std::vector<FlowItem> items = itemsOf(cells_);

	const int availW = std::max(1, width());
	plan_ = planFlow(items, availW, opts_);

	auto widthOf = [&](int id) -> int {
		for (const auto &c : cells_)
			if (c.meta.id == id)
				return flowItemWidth(c.meta);
		return 0;
	};
	auto widgetOf = [&](int id) -> QWidget * {
		for (const auto &c : cells_)
			if (c.meta.id == id)
				return c.w;
		return nullptr;
	};

	// Visibility: everything not collapsed is shown; everything collapsed is
	// hidden (and is reachable through the host's "… More").
	for (const auto &c : cells_) {
		bool collapsed = false;
		for (int id : plan_.collapsed)
			if (id == c.meta.id)
				collapsed = true;
		// An inactive control is the host's to hide; the flow does not
		// bring it back, and never plans a row around it.
		c.w->setVisible(c.active && !collapsed);
	}

	auto minWidthOf = [&](int id) -> int {
		for (const auto &c : cells_)
			if (c.meta.id == id)
				return c.meta.minW;
		return 0;
	};

	int y = 0;
	for (const auto &row : plan_.rows) {
		const int n = (int)row.size();
		if (n == 0)
			continue;
		int rowH = opts_.rowH;
		for (int k = 0; k < n; ++k)
			if (QWidget *w = widgetOf(row[k]))
				rowH = std::max(rowH, w->sizeHint().height());

		std::vector<int> itemW(n);
		int base = 0;
		for (int k = 0; k < n; ++k) {
			itemW[k] = widthOf(row[k]);
			base += itemW[k];
		}
		const int minGaps = std::max(0, n - 1) * opts_.gapX;

		// A ROW WIDER THAN THE CONTAINER IS SHRUNK toward its items'
		// own minimums, latest item first, before it is ever allowed to
		// overflow: a scrollable tab strip or a text key CAN give width
		// back, and a bar that overflows its panel shows a clipped key
		// (measured in the Tall toolbar before this existed).
		int deficit = base + minGaps - width();
		for (int k = n - 1; k >= 0 && deficit > 0; --k) {
			const int slack = itemW[k] - minWidthOf(row[k]);
			if (slack <= 0)
				continue;
			const int take = std::min(slack, deficit);
			itemW[k] -= take;
			deficit -= take;
		}

		int total = minGaps;
		for (int k = 0; k < n; ++k)
			total += itemW[k];

		int gap = opts_.gapX;
		if (n > 1 && total < width()) {
			const int maxAdd = std::max(0, metrics_.laneGapMax - gap);
			gap += std::min(std::max(0, (width() - total) / (n - 1)),
					maxAdd);
			total = (n - 1) * gap;
			for (int k = 0; k < n; ++k)
				total += itemW[k];
		}
		int x = std::max(0, (width() - total) / 2);
		for (int k = 0; k < n; ++k) {
			if (QWidget *w = widgetOf(row[k]))
				w->setGeometry(x, y, itemW[k], rowH);
			x += itemW[k] + gap;
		}
		y += rowH + opts_.gapY;
	}

	// The "… More" sink rides at the end of the last visible row, or on its own
	// row when it does not fit. It is the host's control; this only places it.
	if (more_) {
		if (!plan_.collapsed.empty()) {
			const int mw = std::max(more_->minimumSizeHint().width(),
						more_->sizeHint().width());
			const int mh = std::max(more_->minimumSizeHint().height(),
						more_->sizeHint().height());
			int lastY = 0, lastRight = 0, haveRow = 0;
			for (const auto &row : plan_.rows) {
				haveRow = 1;
				int rw = 0;
				for (std::size_t k = 0; k < row.size(); ++k) {
					if (k)
						rw += opts_.gapX;
					rw += widthOf(row[k]);
				}
				lastRight = (width() + rw) / 2;
				lastY += opts_.rowH + opts_.gapY;
			}
			(void)haveRow;
			int mx = lastRight + opts_.gapX;
			int my = std::max(0, lastY - opts_.rowH - opts_.gapY);
			if (mx + mw > width()) {
				mx = std::max(0, (width() - mw) / 2);
				my = lastY;
			}
			more_->setGeometry(mx, my, mw, mh);
			more_->setVisible(true);
		} else {
			more_->setVisible(false);
		}
	}

	updateGeometry();
}

QSize ResponsiveFlow::sizeHint() const
{
	const std::vector<FlowItem> items = itemsOf(cells_);
	const int w = std::max(flowMinWidth(items), width());
	const int h = naturalHeightForWidth(w);
	return QSize(w, std::max(h, opts_.rowH));
}

QSize ResponsiveFlow::minimumSizeHint() const
{
	const std::vector<FlowItem> items = itemsOf(cells_);
	const int w = flowMinWidth(items);
	const int h = flowMinHeight(items, opts_);
	return QSize(w, h);
}

int ResponsiveFlow::heightForWidth(int w) const
{
	return naturalHeightForWidth(w);
}

void ResponsiveFlow::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	applyPlan();
}

// ---------------------------------------------------------------------------
// ResponsiveSplit
// ---------------------------------------------------------------------------
ResponsiveSplit::ResponsiveSplit(QWidget *parent) : QSplitter(parent)
{
	setChildrenCollapsible(false);
	setHandleWidth(5);
}

void ResponsiveSplit::setPanes(QWidget *first, QWidget *second)
{
	if (first && indexOf(first) < 0)
		addWidget(first);
	if (second && indexOf(second) < 0)
		addWidget(second);
	applyOrientation();
}

void ResponsiveSplit::setExtents(const Extent &first, const Extent &second)
{
	first_ = first;
	second_ = second;
	applyOrientation();
}

SplitAxis ResponsiveSplit::applyOrientation()
{
	const SplitPlan p =
		planSplit(first_, second_, std::max(1, width()), std::max(1, height()),
			  bias_);
	axis_ = p.axis;
	setOrientation(p.axis == SplitAxis::Horizontal ? Qt::Horizontal
						       : Qt::Vertical);
	return axis_;
}

void ResponsiveSplit::resizeEvent(QResizeEvent *e)
{
	QSplitter::resizeEvent(e);
	applyOrientation();
}

// ---------------------------------------------------------------------------
// ResponsiveBand
// ---------------------------------------------------------------------------
ResponsiveBand::ResponsiveBand(QWidget *parent) : QWidget(parent)
{
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	// No QLayout: the engine decides which bands are on screen and this places
	// them, so a QLayout's own height-for-width floor never enters the picture.
}

void ResponsiveBand::setGap(int gapY)
{
	gapY_ = std::max(0, gapY);
	relayout();
}

void ResponsiveBand::addBand(QWidget *w, Priority prio, bool collapsible, bool fill)
{
	if (!w)
		return;
	w->setParent(this);
	Cell c;
	c.w = w;
	c.meta.id = (int)cells_.size() + 1;
	c.meta.prio = prio;
	c.meta.collapsible = collapsible;
	c.meta.fill = fill;
	c.meta.minH = std::max(w->minimumSizeHint().height(), w->minimumHeight());
	c.meta.prefH = std::max(c.meta.minH, w->sizeHint().height());
	cells_.push_back(c);
	relayout();
}

void ResponsiveBand::clearBands()
{
	cells_.clear();
	plan_ = BandPlan{};
	updateGeometry();
}

void ResponsiveBand::setMoreButton(QWidget *b)
{
	more_ = b;
	if (more_)
		more_->setParent(this);
	relayout();
}

void ResponsiveBand::relayout()
{
	applyPlan();
}

void ResponsiveBand::applyPlan()
{
	std::vector<BandItem> items;
	for (const auto &c : cells_)
		items.push_back(c.meta);

	plan_ = planBands(items, height(), gapY_);

	auto visible = [&](int id) {
		for (int v : plan_.visible)
			if (v == id)
				return true;
		return false;
	};

	// Visibility first: a collapsed band is hidden, not destroyed, so bringing
	// it back is a setGeometry() and never a re-parent.
	int fixed = 0, nVis = 0;
	int fillId = 0;
	for (const auto &c : cells_) {
		const bool vis = visible(c.meta.id);
		c.w->setVisible(vis);
		if (!vis)
			continue;
		++nVis;
		if (c.meta.fill)
			fillId = c.meta.id;
		else
			fixed += std::max(c.meta.minH, c.meta.prefH);
	}

	const int gaps = std::max(0, nVis - 1) * gapY_;
	const int leftover = std::max(0, height() - fixed - gaps);

	int y = 0;
	for (const auto &c : cells_) {
		if (!visible(c.meta.id))
			continue;
		int h = std::max(c.meta.minH, c.meta.prefH);
		if (c.meta.id == fillId)
			h = std::max(c.meta.minH, leftover);
		c.w->setGeometry(0, y, width(), h);
		y += h + gapY_;
	}

	if (more_) {
		if (!plan_.collapsed.empty()) {
			const int mw = std::max(more_->minimumSizeHint().width(),
						more_->sizeHint().width());
			const int mh = std::max(more_->minimumSizeHint().height(),
						more_->sizeHint().height());
			const int my = std::min(height() - mh, y > 0 ? y - gapY_ : 0);
			more_->setGeometry(std::max(0, width() - mw), std::max(0, my), mw,
					   mh);
			more_->setVisible(true);
		} else {
			more_->setVisible(false);
		}
	}

	updateGeometry();
}

QSize ResponsiveBand::sizeHint() const
{
	std::vector<BandItem> items;
	for (const auto &c : cells_)
		items.push_back(c.meta);
	const int h = bandsTotalH(items, gapY_);
	int w = 0;
	for (const auto &c : cells_)
		w = std::max(w, std::max(c.w->minimumSizeHint().width(),
					 c.w->sizeHint().width()));
	return QSize(w, h);
}

QSize ResponsiveBand::minimumSizeHint() const
{
	std::vector<BandItem> items;
	for (const auto &c : cells_)
		items.push_back(c.meta);
	// availeH == 1 asks the engine for the maximal collapse: only the bands that
	// refuse to leave remain, and that is this stack's honest floor.
	const int h = planBands(items, 1, gapY_).usedH;
	int w = 0;
	for (const auto &c : cells_)
		w = std::max(w, c.w->minimumSizeHint().width());
	return QSize(w, h);
}

void ResponsiveBand::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	applyPlan();
}

// ---------------------------------------------------------------------------
// ResponsiveFlowItem
// ---------------------------------------------------------------------------
ResponsiveFlowItem::ResponsiveFlowItem(ResponsiveFlow *w) : QWidgetItem(w), flow_(w)
{
}

bool ResponsiveFlowItem::hasHeightForWidth() const
{
	return true;
}

int ResponsiveFlowItem::heightForWidth(int w) const
{
	return flow_->naturalHeightForWidth(w);
}

// THE FLOOR, and the reason this class exists. Qt's default would answer with
// heightForWidth(w) here, i.e. the NATURAL height at this width — the trap that
// made the panel demand the tall shape everywhere.
int ResponsiveFlowItem::minimumHeightForWidth(int) const
{
	return flow_->minimumSizeHint().height();
}

QSize ResponsiveFlowItem::minimumSize() const
{
	return flow_->minimumSizeHint();
}

QSize ResponsiveFlowItem::sizeHint() const
{
	return flow_->sizeHint();
}

void addResponsiveFlow(QBoxLayout *layout, ResponsiveFlow *flow)
{
	if (!layout || !flow)
		return;
	if (QWidget *p = layout->parentWidget(); p && flow->parentWidget() != p)
		flow->setParent(p);
	layout->addItem(new ResponsiveFlowItem(flow));
}

} // namespace ui
} // namespace multireplay
