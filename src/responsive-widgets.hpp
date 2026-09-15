/*
obs-multireplay — responsive widgets
Copyright (C) 2026 obs-multireplay contributors
SPDX-License-Identifier: GPL-2.0-or-later

The Qt skin over the pure engine in responsive-layout.hpp. Two widgets, and
nothing between them and the engine:

  ResponsiveFlow   a wrapping row of controls, with a "… More" host sink
  ResponsiveSplit  a splitter whose orientation the engine chooses

The load-bearing detail is ResponsiveFlowItem: Qt's QWidgetItem derives
minimumHeightForWidth() from heightForWidth(), which is exactly the trap that
made the old strip demand the tall shape's height at every width. This item
answers the floor with the COLLAPSED height instead, so a narrow flow can be
short while still being able to wrap tall when there is room.

The no-reparent invariant of qt-display.hpp holds here: a relayout only calls
setGeometry() and setVisible() on children that are already ours. Nothing is
ever moved to a new parent.
*/

#pragma once

#include "responsive-layout.hpp"

#include <QBoxLayout>
#include <QLayoutItem>
#include <QSize>
#include <QSplitter>
#include <QWidget>
#include <vector>

namespace multireplay {
namespace ui {

// The one metric scale. Normal for a dock viewed at ~60 cm; Gallery for a
// full-screen/maximised panel viewed at 1–2 m. Everything that used to be a
// scattered fixed size comes from here, and the style sheet is handed the same
// numbers so "one place generates the number" holds.
struct Metrics {
	int keyH = 26;
	int keyFoldedH = 22;
	int headerH = 34;
	int zoneGap = 14;
	int bandGap = 4;
	int laneGapMax = 140;
	int captionH = 13;
};

Metrics metricsFor(bool gallery);

// ---------------------------------------------------------------------------
// ResponsiveFlow
// ---------------------------------------------------------------------------
class ResponsiveFlow : public QWidget {
	Q_OBJECT

public:
	explicit ResponsiveFlow(QWidget *parent = nullptr);

	// Add one control. `group` binds consecutive controls that must wrap
	// together; `breakBefore` forces a new row. The flow takes ownership of
	// nothing — the control is re-parented to the flow and laid out by hand.
	void addControl(QWidget *w, int group = 0,
			Priority prio = Priority::Primary, bool collapsible = true,
			bool breakBefore = false);
	// Remove and reparent-away nothing: just forget every control. Used when a
	// zone is rebuilt from scratch.
	void clearControls();

	// A control the HOST has put away (the search key outside Short, the "+"
	// at 20 lists) is not the flow's to show. An inactive control is excluded
	// from the plan and forced hidden; activate it again and it rejoins the
	// wrap. Without this the flow's own setVisible() would override whatever
	// the mode decided.
	void setControlActive(QWidget *w, bool active);
	bool controlActive(const QWidget *w) const;

	// Re-measure every active control from its current sizeHint and lay out
	// again. Needed when a control's own size changes without its text
	// changing shape enough to move its parent: a mode switch that puts a word
	// back on a key, a tab added to the strip.
	void refresh();

	// The widget that receives whatever collapsed (the host's "… More" key).
	// Shown only while something is collapsed; never laid out by the flow.
	void setMoreButton(QWidget *b);

	void setMetrics(const Metrics &m);
	const Metrics &metrics() const { return metrics_; }

	// How many rows the flow would need at `w`, and the height that means. The
	// natural height at width w is what a parent asks for; the collapsed floor
	// is minimumSizeHint().height().
	int naturalRowsForWidth(int w) const;
	int naturalHeightForWidth(int w) const;
	// The plan the flow last applied, for the automated gate.
	const FlowPlan &plan() const { return plan_; }

	// Recompute the plan and place the controls now.
	void relayout();

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	int heightForWidth(int w) const override;
	bool hasHeightForWidth() const override { return true; }

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	struct Cell {
		QWidget *w = nullptr;
		FlowItem meta;
		bool active = true;
	};
	std::vector<Cell> cells_;
	FlowPlan plan_;
	QWidget *more_ = nullptr;
	Metrics metrics_;
	FlowOptions opts_;
	void applyPlan();
};

// ---------------------------------------------------------------------------
// ResponsiveSplit
// ---------------------------------------------------------------------------
// A QSplitter that chooses its own orientation from the engine. It never
// re-parents: setOrientation() does not touch child native windows, which is
// the whole reason a splitter is used here rather than a rebuilt container.
class ResponsiveSplit : public QSplitter {
	Q_OBJECT

public:
	explicit ResponsiveSplit(QWidget *parent = nullptr);

	// Declare the two panes' extents. The splitter places pane 0 first (the
	// pictures) and pane 1 second (the list), and orients itself accordingly.
	void setPanes(QWidget *first, QWidget *second);
	void setExtents(const Extent &first, const Extent &second);
	void setBias(double bias) { bias_ = bias; }
	// Re-evaluate the orientation at the current size. Returns the axis used.
	SplitAxis applyOrientation();
	SplitAxis axis() const { return axis_; }

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	Extent first_, second_;
	double bias_ = 0.0;
	SplitAxis axis_ = SplitAxis::Vertical;
};

// ---------------------------------------------------------------------------
// ResponsiveBand
// ---------------------------------------------------------------------------
// The vertical twin of ResponsiveFlow: a stack of sections that hides the
// least useful ones first when the panel gets short, and gives whatever height
// is left to the one band declared `fill` (the event table). Unlike the flow,
// a band stacks vertically, so its minimum height is the sum of the sections
// that refuse to leave — computable without knowing the width, and therefore
// answerable honestly through minimumSizeHint() with no layout item needed.
class ResponsiveBand : public QWidget {
	Q_OBJECT

public:
	explicit ResponsiveBand(QWidget *parent = nullptr);

	// `prio` orders the collapse (Tertiary leaves first, Critical last).
	// `fill` marks the band that absorbs the leftover height (one only).
	void addBand(QWidget *w, Priority prio = Priority::Primary,
		     bool collapsible = true, bool fill = false);
	void clearBands();
	// The host's "… More" key, shown only while something is collapsed.
	void setMoreButton(QWidget *b);
	void setGap(int gapY);

	// The plan the band last applied, for the automated gate.
	const BandPlan &plan() const { return plan_; }

	void relayout();

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	struct Cell {
		QWidget *w = nullptr;
		BandItem meta;
	};
	std::vector<Cell> cells_;
	BandPlan plan_;
	QWidget *more_ = nullptr;
	int gapY_ = 4;
	void applyPlan();
};

// The layout item that gives a ResponsiveFlow an honest floor. Add the flow to
// a QBoxLayout with addResponsiveFlow(), never with addWidget(): a plain
// QWidgetItem would derive the floor from the natural height-for-width and the
// panel could never shrink into its wrapped shape.
class ResponsiveFlowItem : public QWidgetItem {
public:
	explicit ResponsiveFlowItem(ResponsiveFlow *w);
	bool hasHeightForWidth() const override;
	int heightForWidth(int w) const override;
	int minimumHeightForWidth(int w) const override;
	QSize minimumSize() const override;
	QSize sizeHint() const override;

private:
	ResponsiveFlow *flow_;
};

void addResponsiveFlow(QBoxLayout *layout, ResponsiveFlow *flow);

} // namespace ui
} // namespace multireplay
