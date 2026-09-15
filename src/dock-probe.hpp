#pragma once
// ---------------------------------------------------------------------------
// dock-probe.hpp — THE GEOMETRY THE CHECKS MEASURE, ONE COPY OF IT
// ---------------------------------------------------------------------------
//
// The gate (selftest.cpp) asks these questions of the panel: where a zone sits,
// whether it spans the panel. An earlier harness's --check asked the same ones,
// and a helper copied into each binary is two answers to one question, and two
// answers drift — the same lesson monitorRoomFor (dock-layout.hpp) was written
// for. So the measuring lives here, pure Qt with no OBS types (like
// dock-style.hpp).

#include <QAbstractButton>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QTabBar>
#include <QTableWidget>
#include <QToolButton>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace multireplay::probe {

// The toolbar's box — the one widget that holds all its rows (1 in Wide and
// Short, 3 in Tall). Named in the dock (dock-build.cpp) and looked up by the
// gate.
inline QString toolbarBoxName()
{
	return QStringLiteral("mrToolbarBox");
}

// Where `w` sits, in `panel`'s coordinates. `panel` must be an ancestor of `w`.
inline QRect rectIn(const QWidget *w, const QWidget *panel)
{
	return QRect(w->mapTo(panel, QPoint(0, 0)), w->size());
}

// LAY «Le cinque zone» (1 Toolbar … in cima); SPEC §0 «Cinque zone, dall'alto:
// toolbar · blocco monitor · …». The toolbar starts above the first picture.
inline bool toolbarAboveMonitors(const QWidget *toolbar, const QWidget *monitors,
				 const QWidget *panel)
{
	return rectIn(toolbar, panel).top() < rectIn(monitors, panel).top();
}

// LAY Short: the toolbar crosses the whole panel, not the table's column alone.
// `slack` is the panel's own margins (4 + 4) with room to spare.
inline bool spansPanel(const QWidget *w, const QWidget *panel, int slack = 20)
{
	return w->width() >= panel->width() - slack;
}

// The numbers both checks print, so a red line says where each zone was.
inline QString zoneOrderDetail(const QWidget *toolbar, const QWidget *monitors,
			       const QWidget *panel)
{
	const QRect t = rectIn(toolbar, panel);
	const QRect m = rectIn(monitors, panel);
	return QStringLiteral("toolbar y%1 w%2, monitors y%3, panel w%4")
		.arg(t.top())
		.arg(t.width())
		.arg(m.top())
		.arg(panel->width());
}

// ── THE NOTICE LIVES IN MARCA'S FOOTER (S2, B2, D5) ─────────────────────
// TAS «Le due barre»: under MARCA|REVIEW there is only the SeekBar. The status
// row that used to sit there is gone; its one job left — the answer to a key
// just pressed — is a label beside the health badge. The retired row's name is
// kept here so a check can say it is GONE, not merely hidden.
inline QString statusRowName()
{
	return QStringLiteral("mrStatusBar");
}
inline QString marcaFootName()
{
	return QStringLiteral("mrMarcaFoot");
}
inline QString noticeName()
{
	return QStringLiteral("mrNotice");
}

// The notice label, when it sits inside MARCA's footer; nullptr otherwise.
inline QLabel *noticeInMarcaFoot(const QWidget *panel)
{
	const QWidget *foot = panel->findChild<QWidget *>(marcaFootName());
	return foot ? foot->findChild<QLabel *>(noticeName()) : nullptr;
}

// What the notice SAYS, in full. The label elides a sentence that does not fit
// the footer and carries the whole of it in its tooltip, so the text alone can
// be a shortened copy — a check that looks for a sentence reads this.
inline QString noticeFullText(const QLabel *l)
{
	if (!l)
		return QString();
	return l->toolTip().isEmpty() ? l->text() : l->toolTip();
}

// TALL: MARCA is a tab behind REVIEW, so its footer is out of sight. While the
// footer has something to say and MARCA is not the current tab, the MARCA tab
// title carries « •» and the tab bar's `alert` property (coloured warn by the
// sheet). Tab 1 is MARCA (TwoPanelStrip).
inline bool marcaTabFlagged(const QTabBar *tabs)
{
	return tabs && tabs->count() > 1 &&
	       tabs->tabText(1).endsWith(QStringLiteral(" •")) &&
	       tabs->property("alert").toBool();
}
inline bool marcaTabPlain(const QTabBar *tabs)
{
	return tabs && tabs->count() > 1 &&
	       tabs->tabText(1) == QStringLiteral("MARCA") &&
	       !tabs->property("alert").toBool();
}

// Nothing between the command panel and the position bar: the gap from the
// strip's bottom edge to the bar's top edge. The old status row was 26 px, so
// anything past a few pixels of layout spacing means a row came back.
inline int gapStripToSeek(const QWidget *strip, const QWidget *seek,
			  const QWidget *panel)
{
	return rectIn(seek, panel).top() - rectIn(strip, panel).bottom() - 1;
}

// ── PIXELS ────────────────────────────────────────────────────────────────
// Read back from a grab of the panel. One copy, shared by the gate's colour and
// header checks.

// The most common colour in a region: the fill, whatever it happens to be.
inline QColor dominant(const QImage &img, const QRect &r)
{
	QHash<QRgb, int> counts;
	for (int y = r.top(); y <= r.bottom(); y++)
		for (int x = r.left(); x <= r.right(); x++)
			counts[img.pixel(x, y)]++;
	QRgb best = 0;
	int most = -1;
	for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
		if (it.value() > most) {
			most = it.value();
			best = it.key();
		}
	return QColor::fromRgb(best);
}

inline bool sameColour(const QColor &a, const QColor &b, int tol = 6)
{
	return std::abs(a.red() - b.red()) <= tol &&
	       std::abs(a.green() - b.green()) <= tol &&
	       std::abs(a.blue() - b.blue()) <= tol;
}

// ── THE PANEL HEADERS (K1, K2, R1) ──────────────────────────────────────
// TAS .sub .hd: each of MARCA and REVIEW opens with a header ROW — a rule under
// it, not a box around it — whose title is centred on the row; REVIEW's IN
// OUTPUT sits at the far right. `side` is "mrMarca" or "mrReview"
// (TwoPanelStrip), and the header is that panel's #mrPanelHeader.
inline QWidget *panelHeader(const QWidget *panel, const QString &side)
{
	const QWidget *box = panel->findChild<QWidget *>(side);
	return box ? box->findChild<QWidget *>(QStringLiteral("mrPanelHeader"))
		   : nullptr;
}

inline double centreX(const QRect &r)
{
	return r.left() + r.width() / 2.0;
}
// TAS .fbox{justify-content:center;align-items:center} (K4): the vertical
// centre beside the horizontal one.
inline double centreY(const QRect &r)
{
	return r.top() + r.height() / 2.0;
}
inline int rightEdge(const QRect &r)
{
	return r.left() + r.width();
}

// What the header SHOWS, in `panel`'s coordinates: the union of its labels
// that carry text and, with `withKeys`, its keys. An empty label (REVIEW's
// event with nothing selected) is not content: it has no ink to centre.
inline QRect headerContentRect(const QWidget *header, const QWidget *panel,
			       bool withKeys)
{
	QRect u;
	for (QWidget *c : header->findChildren<QWidget *>()) {
		if (!c->isVisible() || c->width() <= 1)
			continue;
		if (const auto *l = qobject_cast<const QLabel *>(c)) {
			if (l->text().isEmpty())
				continue;
		} else if (!(withKeys && qobject_cast<const QAbstractButton *>(c))) {
			continue;
		}
		u = u.united(rectIn(c, panel));
	}
	return u;
}

// TAS .sub .hd{border-bottom:1px solid} — A RULE, NOT A BOX. Read on a grab of
// the panel (`hr` in the panel's logical coordinates; the image's device pixel
// ratio is honoured): the left tenth of the header, where nothing is laid out
// because both headers centre their content, and its top two rows are the
// ground all the way down to the rule; and the bottom row differs from the
// ground across nine tenths of the header's width. A box drawn around the
// header (its left edge, its top line, its corner) fails the first two.
inline bool headerHasNoFrame(const QImage &shot, const QRect &hr,
			     QString *why = nullptr)
{
	const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;
	const auto dev = [dpr](int v) { return (int)std::floor(v * dpr); };
	const int L = dev(hr.left()), T = dev(hr.top());
	const int R = std::min(shot.width() - 1,
			       (int)std::ceil((hr.left() + hr.width()) * dpr) - 1);
	const int B = std::min(shot.height() - 1,
			       (int)std::ceil((hr.top() + hr.height()) * dpr) - 1);
	// Clean down to two logical rows above the rule.
	const int clean = dev(hr.top() + hr.height() - 2) - 1;
	if (R - L < 40 || clean - T < 8) {
		if (why)
			*why = QStringLiteral("header too small (%1x%2)")
				       .arg(hr.width())
				       .arg(hr.height());
		return false;
	}
	const QColor bg = dominant(shot, QRect(QPoint(L, T), QPoint(R, clean)));
	const auto off = [&](int x, int y) {
		return !sameColour(QColor::fromRgb(shot.pixel(x, y)), bg);
	};
	const int stripR = L + std::max(8, (R - L) / 10);
	for (int y = T; y <= clean; y++)
		for (int x = L; x < stripR; x++)
			if (off(x, y)) {
				if (why)
					*why = QStringLiteral(
						       "ink at the left edge (%1,%2): "
						       "a box, not a rule")
						       .arg(x - L)
						       .arg(y - T);
				return false;
			}
	for (int y = T; y <= T + 1; y++)
		for (int x = L; x <= R; x++)
			if (off(x, y)) {
				if (why)
					*why = QStringLiteral(
						       "ink on the top line at x%1: "
						       "a box, not a rule")
						       .arg(x - L);
				return false;
			}
	int ruled = 0;
	for (int x = L; x <= R; x++)
		if (off(x, B))
			ruled++;
	const bool rule = ruled * 10 >= (R - L + 1) * 9;
	if (why)
		*why = rule ? QStringLiteral("rule only")
			    : QStringLiteral("no rule under it (%1 of %2 px)")
				      .arg(ruled)
				      .arg(R - L + 1);
	return rule;
}

// The MARCA clock: TAS «09:52:20 · rim 01:10:24» — the time of day, no date.
inline bool isClockHms(const QString &s)
{
	if (s.size() != 8)
		return false;
	for (int i = 0; i < 8; i++) {
		const bool colon = i == 2 || i == 5;
		if (colon ? s[i] != QLatin1Char(':') : !s[i].isDigit())
			return false;
	}
	return true;
}

// THE WHOLE HEADER ANSWER: frame, the MARCA group centred, REVIEW's title
// centred, IN OUTPUT at the far right, the clock without a date, and REC the
// compact key. `recKey`/`toOutput` are found
// by the caller (by mrKey) and may be null, which fails. `keyH` is the height
// the compact key must be (kHeaderKeyH, dock-layout.hpp). `detail` says where
// everything was.
inline bool headersConform(const QWidget *panel, const QImage &shot,
			   const QWidget *recKey, const QWidget *toOutput,
			   int keyH, QString *detail)
{
	const QWidget *mh = panelHeader(panel, QStringLiteral("mrMarca"));
	const QWidget *rh = panelHeader(panel, QStringLiteral("mrReview"));
	if (!mh || !rh || !recKey || !toOutput || !mh->isVisible() ||
	    !rh->isVisible()) {
		if (detail)
			*detail = QStringLiteral("marca header %1, review header %2, "
						 "REC %3, IN OUTPUT %4")
					  .arg(mh ? "found" : "MISSING")
					  .arg(rh ? "found" : "MISSING")
					  .arg(recKey ? "found" : "MISSING")
					  .arg(toOutput ? "found" : "MISSING");
		return false;
	}
	const QRect mr = rectIn(mh, panel), rr = rectIn(rh, panel);
	QString mWhy, rWhy;
	const bool mRule = headerHasNoFrame(shot, mr, &mWhy);
	const bool rRule = headerHasNoFrame(shot, rr, &rWhy);
	// TAS .sub.live .hd{justify-content:center}
	const QRect mc = headerContentRect(mh, panel, true);
	const double mOff = centreX(mc) - centreX(mr);
	// TAS .sub.review .hd .st{left:50%;transform:translateX(-50%)}
	const QRect rt = headerContentRect(rh, panel, false);
	const double rOff = centreX(rt) - centreX(rr);
	// TAS .sub.review .hd{justify-content:flex-end}: IN OUTPUT at the far right
	const int outGap = rightEdge(rr) - rightEdge(rectIn(toOutput, panel));
	const QLabel *clock = mh->findChild<QLabel *>(QStringLiteral("mrClock"));
	const bool hms = clock && isClockHms(clock->text());
	// TAS .key.sm{height:24px} — REC in the header is the compact key.
	const bool compact = recKey->height() == keyH;
	if (detail)
		*detail = QStringLiteral(
				  "MARCA %1 (group off centre %2 px), REVIEW %3 "
				  "(title off centre %4 px, IN OUTPUT %5 px from the "
				  "right), clock '%6', REC %7 px")
				  .arg(mRule ? QStringLiteral("rule") : mWhy)
				  .arg(mOff, 0, 'f', 1)
				  .arg(rRule ? QStringLiteral("rule") : rWhy)
				  .arg(rOff, 0, 'f', 1)
				  .arg(outGap)
				  .arg(clock ? clock->text() : QStringLiteral("NO CLOCK"))
				  .arg(recKey->height());
	return mRule && rRule && std::abs(mOff) <= 3.0 && std::abs(rOff) <= 3.0 &&
	       outGap >= 0 && outGap <= 10 && hms && compact && !mc.isEmpty() &&
	       !rt.isEmpty();
}

// ── THE MARCA BOXES (K3, K4) ───────────────────────────────────────────
// TAS .fbox{border:1px solid #3a4a63;border-radius:7px} — always, in every
// form — and .fbox{justify-content:center;align-items:center}: the keys ride
// centred in their box. One copy, like the header helpers above.
//
// `panel` is the dock: the box is found through the key it holds (`keyId`,
// the mrKey of dock-icons.hpp — the literal here so this header stays
// dependency-free), walking up to the ancestor that directly holds a
// mrBlockFrame. No KeyBlock type named: the structure is the contract, not
// the class.
// The bordered frame holding `key`: the ancestor that directly holds a
// mrBlockFrame. No KeyBlock type named: the structure is the contract.
inline QWidget *frameOfKey(const QWidget *key, const QWidget *panel)
{
	if (!key)
		return nullptr;
	for (QWidget *p = key->parentWidget(); p && p != panel;
	     p = p->parentWidget()) {
		if (p->findChild<QWidget *>(QStringLiteral("mrBlockFrame"),
					    Qt::FindDirectChildrenOnly))
			return p;
	}
	return nullptr;
}

inline QWidget *marcaBoxForKey(const QWidget *panel, const char *keyId)
{
	if (!panel || !keyId)
		return nullptr;
	QWidget *key = nullptr;
	for (QWidget *w : panel->findChildren<QWidget *>()) {
		if (w->property("mrKey").toString() ==
		    QString::fromLatin1(keyId)) {
			key = w;
			break;
		}
	}
	return frameOfKey(key, panel);
}

// The bordered frame of a box found above.
inline QWidget *boxFrame(const QWidget *block)
{
	return block ? block->findChild<QWidget *>(
			       QStringLiteral("mrBlockFrame"))
		     : nullptr;
}

// The keys a box shows, in `panel`'s coordinates.
inline QRect keysRect(const QWidget *block, const QWidget *panel)
{
	QRect u;
	if (!block)
		return u;
	for (QWidget *c : block->findChildren<QWidget *>()) {
		if (!c->isVisible() ||
		    !qobject_cast<const QAbstractButton *>(c))
			continue;
		u = u.united(rectIn(c, panel));
	}
	return u;
}

// One logical pixel of `panel` read back from its grab (`shot`), DPR-aware.
// The grab is OF the panel, so shot coordinates are panel coordinates.
inline QColor shotPixel(const QImage &shot, const QWidget *panel, int x,
			int y)
{
	(void)panel;
	const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;
	const int dx = std::min(shot.width() - 1,
				std::max(0, (int)std::floor(x * dpr)));
	const int dy = std::min(shot.height() - 1,
				std::max(0, (int)std::floor(y * dpr)));
	return QColor::fromRgb(shot.pixel(dx, dy));
}

// WCAG relative luminance and contrast ratio: "is this edge there" as a
// number. 1.4 is barely-there, which is the point — a frame nobody can see
// is not a frame.
inline double luminanceOf(const QColor &c)
{
	auto ch = [](double v) {
		return v <= 0.03928 ? v / 12.92
				    : std::pow((v + 0.055) / 1.055, 2.4);
	};
	return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) +
	       0.0722 * ch(c.blueF());
}
inline double contrastRatio(const QColor &a, const QColor &b)
{
	const double la = luminanceOf(a), lb = luminanceOf(b);
	return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// THE WHOLE BOX ANSWER, one copy shared by the gate: the quick-clip box is
// framed (its top edge, right of the legend that interrupts the border at the
// left, separates from the ground inside) and its keys ride centred in it, both
// axes. `detail` says the numbers.
inline bool marcaBoxesConform(const QWidget *panel, const QImage &shot,
			      QString *detail)
{
	QWidget *box = marcaBoxForKey(panel, "mark5");
	QWidget *frame = boxFrame(box);
	if (!box || !frame || !box->isVisible()) {
		if (detail)
			*detail = QStringLiteral("quick-clip box %1, frame %2")
					  .arg(box ? "found" : "MISSING")
					  .arg(frame ? "found" : "MISSING");
		return false;
	}
	const QRect fr = rectIn(frame, panel);
	// TAS .fbox > .flabel{left:12px}: the legend sits on the top-left of
	// the border, so the edge is read right of centre, and the ground 5 px
	// under it — still margin (the frame insets its content), never a key.
	const int px = fr.left() + fr.width() * 3 / 4;
	const QColor edge = shotPixel(shot, panel, px, fr.top());
	const QColor ground = shotPixel(shot, panel, px, fr.top() + 5);
	const double cr = contrastRatio(edge, ground);
	const QRect keys = keysRect(box, panel);
	const double dx = centreX(keys) - centreX(fr);
	const double dy = centreY(keys) - centreY(fr);
	if (detail)
		*detail = QStringLiteral(
				  "frame contrast %1 (edge %2 on %3), keys off "
				  "centre %4/%5 px")
				  .arg(cr, 0, 'f', 2)
				  .arg(edge.name())
				  .arg(ground.name())
				  .arg(dx, 0, 'f', 1)
				  .arg(dy, 0, 'f', 1);
	return cr >= 1.4 && std::abs(dx) <= 3.0 && std::abs(dy) <= 3.0 &&
	       !keys.isEmpty();
}

// ── REVIEW PLAYBACK + MODES (R2, R3) ───────────────────────────────────
// TAS «Riproduzione»: <span class="key play big">▶ PLAY</span><span
// class="key now big">NOW</span>, .key.big{height:42px;min-width:78px};
// TAS .key.now{background:transparent;border-color:var(--sig-rec)} — NOW is
// an outline at rest; TAS .modstack .grp > .key{flex:1} + .key.sm +
// .key.tog — the modes are 24 px equal toggles with their labels whole.
inline bool sameRow(const QWidget *a, const QWidget *b, const QWidget *panel,
		    int tol = 4)
{
	if (!a || !b)
		return false;
	const QRect ra = rectIn(a, panel), rb = rectIn(b, panel);
	return std::abs(ra.top() - rb.top()) <= tol &&
	       std::abs(ra.bottom() - rb.bottom()) <= tol;
}

// Every VISIBLE key in `keys` is `h` tall. Hidden ones (CAM with the tiles
// on screen) are not worn and report stale geometry — a different fault.
inline bool allHeight(const QList<QWidget *> &keys, int h)
{
	for (const QWidget *w : keys) {
		if (!w || !w->isVisible())
			continue;
		if (w->height() != h)
			return false;
	}
	return true;
}

// TAS .modstack .grp > .key{flex:1}: the visible keys sharing a row share
// its width. Grouped by row, so a two-key row and a three-key row are each
// judged on their own.
inline bool equalWidthsPerRow(const QList<QWidget *> &keys,
			      const QWidget *panel)
{
	QList<QWidget *> vis;
	for (QWidget *w : keys) {
		if (w && w->isVisible())
			vis << w;
	}
	while (!vis.isEmpty()) {
		QWidget *first = vis.takeFirst();
		const int w0 = first->width();
		QList<QWidget *> rest;
		for (QWidget *w : vis) {
			if (sameRow(first, w, panel)) {
				if (w->width() != w0)
					return false;
			} else {
				rest << w;
			}
		}
		vis = rest;
	}
	return true;
}

// A label that fits is a whole label: Qt elides with "…" past the edge.
inline bool textFits(const QWidget *w)
{
	const auto *b = qobject_cast<const QAbstractButton *>(w);
	if (!b || !b->isVisible())
		return true;
	const QString t = b->text();
	if (t.isEmpty())
		return true;
	return b->fontMetrics().horizontalAdvance(t) <=
	       b->contentsRect().width();
}
inline bool noTextElided(const QList<QWidget *> &keys)
{
	for (const QWidget *w : keys) {
		if (!textFits(w))
			return false;
	}
	return true;
}

// Find the panel's own keys by mrKey (dock-icons.hpp), without naming the
// dock type. Missing ids are skipped — the caller decides if that fails.
inline QList<QWidget *> keysById(const QWidget *panel,
				 const std::initializer_list<const char *> &ids)
{
	QList<QWidget *> out;
	if (!panel)
		return out;
	for (const char *id : ids) {
		for (QWidget *w : panel->findChildren<QWidget *>()) {
			if (w->property("mrKey").toString() ==
			    QString::fromLatin1(id)) {
				out << w;
				break;
			}
		}
	}
	return out;
}

// THE WHOLE PLAYBACK ANSWER, one copy shared by the gate.
inline bool reviewConform(const QWidget *panel, const QImage &shot,
			  const QWidget *playKey, const QWidget *nowKey,
			  const QList<QWidget *> &modeKeys, QString *detail)
{
	if (!playKey || !nowKey) {
		if (detail)
			*detail = QStringLiteral("PLAY %1, NOW %2")
					  .arg(playKey ? "found" : "MISSING")
					  .arg(nowKey ? "found" : "MISSING");
		return false;
	}
	const auto *play = qobject_cast<const QAbstractButton *>(playKey);
	const bool big = playKey->height() == 42 && playKey->width() >= 78 &&
			 play && play->text().contains(QStringLiteral("PLAY"));
	const bool sameSize = nowKey->height() == playKey->height() &&
			      nowKey->width() == playKey->width();
	// TAS .key.now{background:transparent}: the key shows its ground. Read
	// off-text (the centred word would be the sample) against the box's own
	// ground, same point Task 5 reads the MARCA frame at.
	QWidget *frame = frameOfKey(nowKey, panel);
	const QRect nr = rectIn(nowKey, panel);
	const QRect fr = frame ? rectIn(boxFrame(frame), panel) : QRect();
	const bool outline =
		!fr.isEmpty() &&
		sameColour(shotPixel(shot, panel, nr.left() + 8,
				     (int)centreY(nr)),
			   shotPixel(shot, panel, fr.left() + fr.width() * 3 / 4,
				     fr.top() + 5));
	const bool modesH = allHeight(modeKeys, 24);
	const bool modesW = equalWidthsPerRow(modeKeys, panel);
	const bool modesT = noTextElided(modeKeys);
	const bool modes = modesH && modesW && modesT;
	if (detail) {
		QString hs, ws, cut;
		for (const QWidget *w : modeKeys) {
			if (!w || !w->isVisible())
				continue;
			hs += QStringLiteral("%1:%2 ")
				      .arg(w->property("mrKey").toString())
				      .arg(w->height());
			ws += QStringLiteral("%1 ")
				      .arg(w->width());
			if (!textFits(w))
				cut = w->property("mrKey").toString();
		}
		*detail = QStringLiteral(
				  "PLAY %1x%2 '%3', NOW %4x%5 %6, modes h[%7] "
				  "w[%8] cut:%9")
				  .arg(playKey->height())
				  .arg(playKey->width())
				  .arg(play ? play->text() : QStringLiteral("?"))
				  .arg(nowKey->height())
				  .arg(nowKey->width())
				  .arg(outline ? QStringLiteral("outline")
					       : QStringLiteral("FILLED"))
				  .arg(hs.trimmed())
				  .arg(ws.trimmed())
				  .arg(cut.isEmpty() ? QStringLiteral("-") : cut);
	}
	return big && sameSize && outline && modes;
}

// ── TRANSPORT, TRIM, SPEED (R4, R5, R6) ─────────────────────────────────
// TAS «Trasporto»: ⏮ ⏭ · <span class="spacer">16px</span> · ◀ ▶ ■ — the
// margin between the frame steps and reverse is declared air, not a hole
// (a hole collapses; a 16 px spacer does not).
inline int gapBetween(const QWidget *a, const QWidget *b, const QWidget *panel)
{
	// Air pixels between them: rightEdge is the EXCLUSIVE edge
	// (left + width), so no minus one — that off-by-one cost a run that
	// read 19 px of a 20 px margin.
	const QRect ra = rectIn(a, panel), rb = rectIn(b, panel);
	return rb.left() - rightEdge(ra); // b right of a
}

// TAS «Rifinitura»/«Velocità»: a key's fill, read off a grab — off-text
// (the centred word would be the sample, the Task 6 NOW trap).
inline QColor chipColour(const QWidget *key, const QImage &shot,
			 const QWidget *panel)
{
	if (!key)
		return QColor();
	const QRect r = rectIn(key, panel);
	return shotPixel(shot, panel, r.left() + 4, (int)centreY(r));
}

// TAS «Velocità» (R6): .seg (chips without %) + spacer + .track.vel with
// the ends labelled 25/125 + readout — on ONE row.
inline bool speedConform(const QWidget *panel, const QImage &shot,
			 const QList<QWidget *> &chips, const QWidget *slider,
			 const QWidget *lo, const QWidget *hi, QString *detail)
{
	if (chips.isEmpty() || !slider || !lo || !hi) {
		if (detail)
			*detail = QStringLiteral("chips %1, slider %2, ends %3/%4")
					  .arg(chips.size())
					  .arg(slider ? "found" : "MISSING")
					  .arg(lo ? "found" : "MISSING")
					  .arg(hi ? "found" : "MISSING");
		return false;
	}
	const auto *b0 = qobject_cast<const QAbstractButton *>(chips.first());
	const bool noPct = b0 && b0->text() == QStringLiteral("25");
	const QLabel *loL = qobject_cast<const QLabel *>(lo);
	const QLabel *hiL = qobject_cast<const QLabel *>(hi);
	const bool ends = loL && hiL && loL->text() == QStringLiteral("25") &&
			  hiL->text() == QStringLiteral("125");
	bool row = true;
	for (QWidget *c : chips)
		row = row && sameRow(c, slider, panel);
	if (detail) {
		QString cw;
		for (QWidget *c : chips) {
			const auto *b =
				qobject_cast<const QAbstractButton *>(c);
			cw += QStringLiteral("'%1' ")
				      .arg(b ? b->text() : QStringLiteral("?"));
		}
		*detail = QStringLiteral("chips [%1] sliderow %2 ends %3/%4")
				  .arg(cw.trimmed())
				  .arg(row ? QStringLiteral("one")
					   : QStringLiteral("SPLIT"))
				  .arg(loL ? loL->text() : QStringLiteral("?"))
				  .arg(hiL ? hiL->text() : QStringLiteral("?"));
	}
	return row && noPct && ends;
}

// ── THE ON-AIR BAND (R7) ───────────────────────────────────────────────
// TAS .band{justify-content:center} + .band .edge{position:absolute;
// right:10px}: the text centred on the WHOLE band, ≫ a bare glyph over its
// right end — borderless, 40% white at rest, solid while on air.
//
// The text is PAINT, not a widget, so centring is read off pixels: ink =
// pixels far (in lightness) from the band's own fill, scanned on the middle
// rows but outside the >> key and outside the white sequence joins (read on
// the text-free top/bottom rows first — they cross the full height, the
// text does not).
inline bool bandConform(const QWidget *panel, const QImage &shot,
			const QWidget *band, const QWidget *skip, QString *detail)
{
	if (!band || !skip || !band->isVisible()) {
		if (detail)
			*detail = QStringLiteral("band %1, skip %2")
					  .arg(band ? "found" : "MISSING")
					  .arg(skip ? "found" : "MISSING");
		return false;
	}
	const QRect br = rectIn(band, panel);
	const QRect sr = rectIn(skip, panel);
	const auto px = [&](int x, int y) {
		return shotPixel(shot, panel, x, y);
	};
	const auto lum = [](const QColor &c) { return luminanceOf(c); };
	// The fill, from a corner the text never reaches (vertically centred
	// paint stays clear of the top rows); most common wins, so a join
	// crossing the corner cannot take it. Sampled one pixel at a time:
	// dominant() reads device pixels, this helper is logical.
	QHash<QRgb, int> fills;
	for (int y = br.top(); y <= br.top() + 4; y++)
		for (int x = br.left(); x <= br.left() + 15; x++)
			fills[px(x, y).rgba()]++;
	QRgb best = 0;
	int most = -1;
	for (auto it = fills.constBegin(); it != fills.constEnd(); ++it)
		if (it.value() > most) {
			most = it.value();
			best = it.key();
		}
	const QColor fill = QColor::fromRgba(best);
	const auto isInk = [&](const QColor &c) {
		return std::abs(lum(c) - lum(fill)) * 255.0 >= 60.0;
	};
	// Joins first, on the text-free rows.
	QList<int> joinX;
	for (int y : {br.top() + 2, br.bottom() - 2}) {
		for (int x = br.left(); x <= br.right(); x++) {
			if (x >= sr.left() && x <= sr.right())
				continue;
			if (isInk(px(x, y)))
				joinX << x;
		}
	}
	const auto nearJoin = [&](int x) {
		for (int jx : joinX) {
			if (std::abs(x - jx) <= 1)
				return true;
		}
		return false;
	};
	// Then the text, on the middle rows.
	int L = br.right(), R = br.left(), n = 0;
	for (int y = br.top() + br.height() / 2 - 7;
	     y <= br.top() + br.height() / 2 + 7; y++) {
		for (int x = br.left(); x <= br.right(); x++) {
			if ((x >= sr.left() && x <= sr.right()) || nearJoin(x))
				continue;
			if (isInk(px(x, y))) {
				L = std::min(L, x);
				R = std::max(R, x);
				n++;
			}
		}
	}
	const double off = (L <= R) ? ((L + R) / 2.0 - centreX(br)) : 999.0;
	// >>: no border of its own. Compared horizontally, inside against
	// just outside at mid-height: the band's fill runs full-height but
	// varies along x (played vs remainder), so a vertical comparison
	// would read the progress edge as a frame. The inside sample sits 2 px
	// in — clear of the centred 16 px mark — the outside one 2 px out.
	const int cy = sr.top() + sr.height() / 2;
	const bool bare = sameColour(px(sr.left() - 2, cy), px(sr.left() + 2, cy));
	// ...and the ink the band's state calls for: solid white on air, 40%
	// at rest. The rest pixmap composites over the fill, so it reads
	// between fill and white — never white, never fill.
	QColor mark(0, 0, 0);
	int mn = 0;
	for (int y = sr.top() + 2; y <= sr.bottom() - 2; y++)
		for (int x = sr.left() + 2; x <= sr.right() - 2; x++) {
			const QColor c = px(x, y);
			if (lum(c) > lum(mark)) {
				mark = c;
				mn++;
			}
		}
	const bool live = skip->property("live").toBool();
	const QColor white(255, 255, 255);
	const bool inkOk = mn > 0 && (live ? sameColour(mark, white, 30)
					  : !sameColour(mark, white, 30) &&
						    lum(mark) > lum(fill) + 0.1);
	if (detail)
		*detail = QStringLiteral("text %1 px centred %2, >> %3, mark %4 (%5)")
				  .arg(n)
				  .arg(off, 0, 'f', 1)
				  .arg(bare ? QStringLiteral("bare")
					    : QStringLiteral("FRAMED"))
				  .arg(mark.name())
				  .arg(live ? QStringLiteral("live")
					    : QStringLiteral("rest"));
	return n > 0 && std::abs(off) <= 3.0 && bare && inkOk;
}

// ── THE MONITOR ROW (M1) ───────────────────────────────────────────────
// TAS MON .mrow{gap:6px}: A, B and the tile grid ADJACENT — images, not
// boxes: a box wider than its picture centres it and the gap reads as a
// black band. Whatever slack the row cannot fill trails at its end, never
// between two pictures.
//
// `pics` are the PICTURE widgets (AspectBox::picture()) in any order; rows
// are read off y. `paneW` is the row's pane width.
// ── THE MONITOR ROW (M1) ───────────────────────────────────────────────
// TAS MON .mrow{gap:6px}: A, B and the tile grid ADJACENT — boxes, whose
// pictures fill them. Two halves, because they are two different faults:
// boxes apart (slack parked between groups) and pictures afloat inside
// their boxes (a box wider than its image centres it: the black band).
// The pictures alone cannot tell them apart: every box draws a 2–3 px
// tally ring inside its edge, so adjacent boxes (6 px) always read 10–12 px
// picture-to-picture — demanding ≤8 there fails the drawing itself
// (measured). `pics`/`boxes` run parallel (AspectBox::picture() of each).
inline bool monitorRowConform(const QWidget *panel, const QList<QWidget *> &pics,
			      const QList<QWidget *> &boxes, QString *detail)
{
	if (pics.size() != boxes.size() || pics.size() < 2) {
		if (detail)
			*detail = QStringLiteral("pics %1 boxes %2")
					  .arg(pics.size())
					  .arg(boxes.size());
		return false;
	}
	struct Item {
		QWidget *pic;
		QWidget *box;
	};
	QList<Item> vis;
	for (int i = 0; i < pics.size(); i++) {
		QWidget *p = pics[i];
		QWidget *b = boxes[i];
		// Filtered as PAIRS: dropping only one side scrambles the
		// rest (a hidden B picture against a filtered B box read as
		// "pics 3 boxes 2" for a whole session).
		if (!p || !b || !p->isVisible() || !b->isVisible() ||
		    p->width() <= 0)
			continue;
		vis << Item{p, b};
	}
	if (vis.size() < 2) {
		if (detail)
			*detail = QStringLiteral("only %1 pictures").arg(vis.size());
		return vis.size() == 1;
	}
	std::sort(vis.begin(), vis.end(), [panel](const Item &a, const Item &b) {
		const QRect ra = rectIn(a.pic, panel), rb = rectIn(b.pic, panel);
		if (ra.top() != rb.top())
			return ra.top() < rb.top();
		return ra.left() < rb.left();
	});
	// Rows are vertical bands (tops within a few px would miss a short
	// tile beside a tall bay).
	QList<QList<Item>> rows;
	for (const Item &it : vis) {
		const QRect r = rectIn(it.pic, panel);
		bool placed = false;
		for (QList<Item> &row : rows) {
			const QRect first = rectIn(row.first().pic, panel);
			if (r.top() < first.bottom()) {
				row << it;
				placed = true;
				break;
			}
		}
		if (!placed)
			rows << QList<Item>{it};
	}
	int worstGap = -999;
	bool overlap = false;
	int pairs = 0;
	int worstFill = 0;
	for (QList<Item> row : rows) {
		std::sort(row.begin(), row.end(),
			  [panel](const Item &a, const Item &b) {
				  return rectIn(a.pic, panel).left() <
					 rectIn(b.pic, panel).left();
			  });
		for (int i = 0; i < row.size(); i++) {
			const QRect br = rectIn(row[i].box, panel);
			const QRect pr = rectIn(row[i].pic, panel);
			// Width only: M1 is the horizontal band between images.
			// A few px of vertical letterbox (tall row, 16:9 picture)
			// is panel-coloured air, not a band between neighbours.
			// Budget 12, not 8: boxes draw 2–3 px tally rings inside
			// their edge, and a column of mixed rings (on-air 3 px
			// beside watched 2 px) legitimately differs by more than
			// the rings alone — measured 9. Anything structural (the
			// 342 px this task started from) is an order above.
			worstFill = std::max(worstFill, br.width() - pr.width());
			if (i == 0)
				continue;
			// Side by side only (see the stacked-tiles note above).
			const QRect p = rectIn(row[i - 1].pic, panel);
			const QRect r = pr;
			const int vOverlap =
				std::min(p.bottom(), r.bottom()) -
				std::max(p.top(), r.top());
			if (vOverlap <= 0)
				continue;
			// ...but the GAP is the boxes': the rings live inside.
			const QRect pb = rectIn(row[i - 1].box, panel);
			const QRect rb2 = br;
			const int g = rb2.left() - rightEdge(pb);
			worstGap = std::max(worstGap, g);
			if (g < 0)
				overlap = true;
			pairs++;
		}
	}
	if (detail)
		*detail = QStringLiteral(
				  "%1 pictures in %2 rows, worst box gap %3, worst fill %4%5")
				  .arg(vis.size())
				  .arg(rows.size())
				  .arg(worstGap)
				  .arg(worstFill)
				  .arg(overlap ? QStringLiteral(" OVERLAP")
					       : QStringLiteral(""));
	return pairs > 0 && !overlap && worstGap <= 8 && worstFill <= 12;
}

// ── EVENT TABLE (E1–E8, D1, D2) ───────────────────────────────────────────
// TAB «la tabella eventi»: no zebra (.er — only the selected row wears a
// fill), selection in D1 orange, K1 speed only on override, W2 plain-text
// comment with a chip popover. One copy shared by the gate.

// TAB .er (E1): rows without alternation — only the selected one has a fill.
inline bool tableHasNoZebra(const QTableWidget *t)
{
	return t && !t->alternatingRowColors();
}

// TAB D1 (E2): the resolved sheet's answer for the selected row. `hex` is
// the scheme's rowSel, so every theme answers its own adapted orange.
inline bool sheetSelectsRow(const QString &sheet, const QString &hex)
{
	const int at = sheet.indexOf(
		QStringLiteral("QTableWidget#mrEvents::item:selected"));
	return at >= 0 &&
	       sheet.mid(at, 400).contains(hex, Qt::CaseInsensitive);
}

// TAB K1 (E3): the speed badge prints only an override. The no-override
// state is the empty string — never "--" (that word lives in the speed
// menu, where it IS a choice). `camCol` is the table's first camera
// column; the caller owns the column numbers, this header stays free of
// the dock type.
inline QString cellSpeedText(const QTableWidget *t, int row, int camCol)
{
	if (!t)
		return QStringLiteral("?");
	if (QWidget *cell = t->cellWidget(row, camCol))
		if (const QPushButton *sp = cell->findChild<QPushButton *>(
			    QStringLiteral("mrAngleSpeed")))
			return sp->text();
	return QStringLiteral("?");
}

// TAB W2, D2 (E4): at rest the comment cell is plain text — one flat key,
// no frame, no chooser beside it.
inline bool noteCellIsPlainText(const QWidget *cell)
{
	if (!cell)
		return false;
	if (!cell->findChild<QPushButton *>(QStringLiteral("mrNoteText")))
		return false;
	if (cell->findChild<QLineEdit *>())
		return false;
	if (cell->findChild<QPushButton *>(QStringLiteral("mrNotePick")))
		return false;
	return true;
}

// TAB W2, D2: the popover one click opens — preset chips over a free-text
// field, recognised by name so the gate finds the dock's.
inline bool notePopoverHasChips(const QWidget *popover)
{
	if (!popover)
		return false;
	if (popover->objectName() != QStringLiteral("mrNotePopover"))
		return false;
	if (popover->findChildren<QPushButton *>().isEmpty())
		return false;
	return popover->findChild<QLineEdit *>() != nullptr;
}

// ── TOOLBAR (T1–T8) ────────────────────────────────────────────────────
// TB «la toolbar»: ~5 banks + pinned +, then scroll inside the strip;
// search a field (icon only in Short); ⛶▾ with its chevron; LIVE whole;
// Monitors with its word, in every form.

// TB .tb-tabs{max-width:300px} (T1): the strip is capped — the slack lives
// between the banks and the tools, not inside the strip (revoked §9b).
inline bool bankStripCapped(const QWidget *bankRow, int cap)
{
	return bankRow && bankRow->width() <= cap;
}

// TB .fade{width:24px} (T1): the strip ends in a 24px fade over its right
// end — a wheel-less hint that it scrolls inside. A plain QWidget named
// mrBankFade, transparent to the mouse so the tabs stay clickable under it.
inline bool bankStripHasFade(const QWidget *bankRow, QString *detail = nullptr)
{
	auto fail = [&](const QString &why) {
		if (detail)
			*detail = why;
		return false;
	};
	if (!bankRow)
		return fail(QStringLiteral("no strip"));
	const QWidget *tabs =
		bankRow->findChild<QWidget *>(QStringLiteral("mrListTabs"));
	if (!tabs)
		return fail(QStringLiteral("no mrListTabs"));
	const QWidget *fade =
		bankRow->findChild<QWidget *>(QStringLiteral("mrBankFade"));
	if (!fade)
		return fail(QStringLiteral("no mrBankFade"));
	if (!fade->isVisibleTo(bankRow))
		return fail(QStringLiteral("fade hidden"));
	if (std::abs(fade->width() - 24) > 1)
		return fail(QStringLiteral("fade w%1").arg(fade->width()));
	const int tabsRight = tabs->x() + tabs->width();
	const int fadeRight =
		fade->mapTo(const_cast<QWidget *>(bankRow), QPoint(0, 0)).x() +
		fade->width();
	if (std::abs(tabsRight - fadeRight) > 2)
		return fail(QStringLiteral("fade ends %1, tabs %2")
				    .arg(fadeRight)
				    .arg(tabsRight));
	if (!fade->testAttribute(Qt::WA_TransparentForMouseEvents))
		return fail(QStringLiteral("fade eats clicks"));
	if (detail)
		*detail = QStringLiteral("fade 24px over the tabs' end");
	return true;
}

// SPEC §9 (T4): the layout key wears ⛶ WITH its chevron — one drawn mark,
// not the platform's arrow (no rule reaches that). Read off the icon
// pixmap itself: ink on the left two thirds (the corners) and ink in the
// right sixth (the chevron). Fractions, so DPI scaling cannot move them.
inline bool layoutKeyHasChevron(const QAbstractButton *key,
				QString *detail = nullptr)
{
	if (!key || key->icon().isNull()) {
		if (detail)
			*detail = QStringLiteral("no icon");
		return false;
	}
	const QPixmap pm = key->icon().pixmap(key->iconSize());
	if (pm.isNull() || pm.width() < 6) {
		if (detail)
			*detail = QStringLiteral("no pixmap");
		return false;
	}
	const QImage img = pm.toImage().convertToFormat(
		QImage::Format_ARGB32);
	bool left = false, right = false;
	for (int y = 0; y < img.height(); y++) {
		for (int x = 0; x < img.width(); x++) {
			if (qAlpha(img.pixel(x, y)) < 32)
				continue;
			if (x < img.width() * 2 / 3)
				left = true;
			if (x > img.width() * 5 / 6)
				right = true;
		}
	}
	if (detail)
		*detail = QStringLiteral("corners %1, chevron %2")
				  .arg(left)
				  .arg(right);
	return left && right;
}

// TB «campo sempre, icona solo in Short» (T3): outside Short the field
// stands alone; in Short the key stands alone until tapped.
inline bool searchConform(const QWidget *panel, const QString &form,
			  QString *detail = nullptr)
{
	if (!panel) {
		if (detail)
			*detail = QStringLiteral("no panel");
		return false;
	}
	const QWidget *field =
		panel->findChild<QWidget *>(QStringLiteral("mrSearch"));
	const QWidget *lens =
		panel->findChild<QWidget *>(QStringLiteral("mrSearchKey"));
	const bool fieldOut = field && field->isVisibleTo(panel);
	const bool lensOut = lens && lens->isVisibleTo(panel);
	if (detail)
		*detail = QStringLiteral("field %1, key %2")
				  .arg(fieldOut)
				  .arg(lensOut);
	if (form == QStringLiteral("short"))
		return !fieldOut && lensOut;
	return fieldOut && !lensOut;
}

} // namespace multireplay::probe
