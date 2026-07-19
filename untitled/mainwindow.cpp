#include "mainwindow.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFrame>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QPainter>
#include <QFont>
#include <QDateTime>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTabWidget>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QMap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <cmath>

// ============================================================
//  StatusDelegate — preserves status-cell background color
//  when a row is selected (overrides the default blue highlight)
// ============================================================
class StatusDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Fill with the stored background color (teal / gold / rose)
        QVariant bg = index.data(Qt::BackgroundRole);
        p->fillRect(option.rect, bg.isValid() ? bg.value<QColor>() : Qt::white);

        // Draw a pink selection border instead of a full highlight
        if (option.state & QStyle::State_Selected) {
            p->save();
            p->setPen(QPen(QColor("#ff4da6"), 2));
            p->drawRect(option.rect.adjusted(1, 1, -2, -2));
            p->restore();
        }

        // Draw the status text in bold white
        QVariant fg = index.data(Qt::ForegroundRole);
        p->setPen(fg.isValid() ? fg.value<QColor>() : QColor("#222222"));
        QFont f = option.font;
        f.setBold(true);
        p->setFont(f);
        p->drawText(option.rect, Qt::AlignCenter,
                    index.data(Qt::DisplayRole).toString());
    }
};

// ============================================================
//  BarChartWidget — animated bar chart (bars rise from baseline)
//  animProgress: 0.0 = all bars at zero height, 1.0 = full height
// ============================================================
class BarChartWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double animProgress READ animProgress WRITE setAnimProgress)

public:
    QString title;
    QList<QPair<QString, double>> data;
    QList<QColor> palette;

    explicit BarChartWidget(const QString &t, QWidget *parent = nullptr)
        : QWidget(parent), title(t), m_progress(1.0)
    {
        setMinimumHeight(190);
        setMaximumHeight(220);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        // Default palette — pink (used by Products per Category chart)
        palette = {
            QColor("#cc0066"),  // deep pink
            QColor("#ff4da6"),  // bright pink
            QColor("#FF75BB"),  // light pink
            QColor("#e6008a"),  // hot pink
            QColor("#ff99cc"),  // soft pink
            QColor("#ff1a8c"),  // vivid pink
            QColor("#ffb3d9"),  // pale pink
            QColor("#990050"),  // dark rose
            QColor("#ff66b3"),  // medium pink
            QColor("#ffd6ec"),  // blush
        };
    }

    // Call this to switch to a different color scheme
    void setColorScheme(const QString &scheme)
    {
        if (scheme == "purple") {
            palette = {
                QColor("#6a0dad"),  // deep purple
                QColor("#9b30d9"),  // vivid purple
                QColor("#b66fe0"),  // medium purple
                QColor("#7c3aed"),  // violet
                QColor("#a855f7"),  // soft violet
                QColor("#c084fc"),  // light violet
                QColor("#4c0080"),  // dark plum
                QColor("#8b5cf6"),  // periwinkle violet
                QColor("#d8b4fe"),  // pale lavender
                QColor("#5b21b6"),  // indigo purple
            };
        }
    }

    double animProgress() const { return m_progress; }
    void setAnimProgress(double v) { m_progress = v; update(); }

    // One-shot rise animation: bars grow from baseline up, triggered once per data load
    void setData(const QList<QPair<QString, double>> &d)
    {
        data = d;
        m_progress = 0.0;

        QPropertyAnimation *anim = new QPropertyAnimation(this, "animProgress", this);
        anim->setDuration(750);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

private:
    double m_progress;   // 0.0 – 1.0 controls bar height during animation

    // Truncate a label to fit within maxPx pixels using given font metrics
    static QString truncLabel(const QString &lbl, const QFontMetrics &fm, int maxPx)
    {
        if (fm.horizontalAdvance(lbl) <= maxPx) return lbl;
        QString s = lbl;
        while (s.length() > 1 && fm.horizontalAdvance(s + "...") > maxPx)
            s.chop(1);
        return s + "...";
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int W = width(), H = height();
        int padL = 48, padR = 16, padT = 36, padB = 48;
        int chartW = W - padL - padR;
        int chartH = H - padT - padB;

        // Background with border
        p.fillRect(rect(), QColor("#fdf0f7"));
        p.setBrush(QColor("#fffafd"));
        p.setPen(QPen(QColor("#d4789c"), 2));
        p.drawRoundedRect(4, 4, W - 8, H - 8, 12, 12);

        // Title
        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(QColor("#880e4f"));
        p.drawText(0, 10, W, 22, Qt::AlignCenter, title);

        if (data.isEmpty()) {
            p.setFont(QFont("Arial", 9));
            p.setPen(QColor("#c4a0b4"));
            p.drawText(rect(), Qt::AlignCenter, "No data");
            return;
        }

        double maxVal = 0;
        double totalVal = 0;
        for (auto &kv : data) { maxVal = qMax(maxVal, kv.second); totalVal += kv.second; }
        if (maxVal == 0) maxVal = 1;
        if (totalVal == 0) totalVal = 1;

        // Grid lines — soft dusty rose tint
        int gridLines = 4;
        p.setFont(QFont("Arial", 7));
        for (int i = 0; i <= gridLines; i++) {
            double val = maxVal * i / gridLines;
            int y = padT + chartH - (int)(chartH * i / gridLines);
            p.setPen(QPen(QColor("#f5d0e3"), 1, Qt::DashLine));
            p.drawLine(padL, y, padL + chartW, y);
            p.setPen(QColor("#9c2060"));
            p.drawText(0, y - 7, padL - 3, 14,
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number((int)val));
        }

        int n    = data.size();
        int barW = qMax(10, (chartW / n) - 10);
        int gap  = (chartW - barW * n) / (n + 1);

        QFont lblFont("Arial", 7);
        QFontMetrics fm(lblFont);

        for (int i = 0; i < n; i++) {
            double  val     = data[i].second;
            QString lbl     = data[i].first;
            QColor  col     = palette[i % palette.size()];

            int barH = (int)(chartH * val / maxVal * m_progress);
            int x    = padL + gap + i * (barW + gap);
            int y    = padT + chartH - barH;

            if (barH > 0) {
                // Drop shadow
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 12));
                p.drawRoundedRect(x + 2, y + 2, barW, barH, 5, 5);

                // Gradient bar — lighter top, richer bottom
                QLinearGradient grad(x, y, x, y + barH);
                grad.setColorAt(0.0, col.lighter(140));
                grad.setColorAt(0.5, col);
                grad.setColorAt(1.0, col.darker(120));
                p.setBrush(grad);
                p.setPen(QPen(col.darker(130), 1));
                p.drawRoundedRect(x, y, barW, barH, 5, 5);

                // Shine strip at top of bar
                p.setBrush(QColor(255, 255, 255, 55));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(x + 2, y + 2, barW - 4, qMin(barH / 3, 10), 4, 4);
            }

            // Value + percentage label above bar (fades in as bars finish rising)
            if (m_progress > 0.82 && barH > 0) {
                int pct = (int)(val / totalVal * 100.0);
                p.setFont(QFont("Arial", 7, QFont::Bold));
                p.setPen(QColor("#4a0026"));
                p.drawText(x - 4, y - 26, barW + 8, 14, Qt::AlignCenter,
                           QString::number((int)val));
                p.setPen(QColor("#9c2060"));
                p.drawText(x - 4, y - 13, barW + 8, 12, Qt::AlignCenter,
                           QString("%1%").arg(pct));
            }

            // X-axis label
            QString shortened = truncLabel(lbl, fm, barW + gap - 2);
            p.setFont(lblFont);
            p.setPen(QColor("#9c2060"));
            p.drawText(x - gap/2, padT + chartH + 4, barW + gap, 18,
                       Qt::AlignHCenter | Qt::AlignTop, shortened);
        }

        // Axes
        p.setPen(QPen(QColor("#d4789c"), 1.5));
        p.drawLine(padL, padT, padL, padT + chartH);
        p.drawLine(padL, padT + chartH, padL + chartW, padT + chartH);
    }
};

// ============================================================
//  LineChartWidget — animated line chart (line draws left→right)
//  animProgress: 0.0 = nothing drawn, 1.0 = fully drawn
// ============================================================
class LineChartWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double animProgress READ animProgress WRITE setAnimProgress)

public:
    QString title;
    QList<QPair<QString, double>> data;

    explicit LineChartWidget(const QString &t, QWidget *parent = nullptr)
        : QWidget(parent), title(t), m_progress(1.0)
    {
        setMinimumHeight(160);
        setMaximumHeight(190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    double animProgress() const { return m_progress; }
    void setAnimProgress(double v) { m_progress = v; update(); }

    void setData(const QList<QPair<QString, double>> &d)
    {
        data = d;
        m_progress = 0.0;

        QPropertyAnimation *anim = new QPropertyAnimation(this, "animProgress", this);
        anim->setDuration(900);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

private:
    double m_progress;

    static QString truncLabel(const QString &lbl, const QFontMetrics &fm, int maxPx)
    {
        if (fm.horizontalAdvance(lbl) <= maxPx) return lbl;
        QString s = lbl;
        while (s.length() > 1 && fm.horizontalAdvance(s + "...") > maxPx)
            s.chop(1);
        return s + "...";
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int W = width(), H = height();
        int padL = 50, padR = 18, padT = 34, padB = 42;
        int chartW = W - padL - padR;
        int chartH = H - padT - padB;

        // Background with border — warm plum tint
        QLinearGradient bgGrad(0, 0, 0, H);
        bgGrad.setColorAt(0, QColor("#fdf0f7"));
        bgGrad.setColorAt(1, QColor("#f8e4f2"));
        p.fillRect(rect(), bgGrad);
        p.setBrush(QColor("#fffafd"));
        p.setPen(QPen(QColor("#d4789c"), 2));
        p.drawRoundedRect(4, 4, W - 8, H - 8, 12, 12);

        // Title
        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(QColor("#880e4f"));
        p.drawText(0, 10, W, 20, Qt::AlignCenter, title);

        if (data.size() < 2) {
            p.setFont(QFont("Arial", 9));
            p.setPen(QColor("#c4a0b4"));
            p.drawText(rect(), Qt::AlignCenter, "Not enough data");
            return;
        }

        double maxVal = 0, totalVal = 0;
        for (auto &kv : data) { maxVal = qMax(maxVal, kv.second); totalVal += kv.second; }
        if (maxVal == 0) maxVal = 1;
        if (totalVal == 0) totalVal = 1;

        // Grid lines — soft dusty rose
        int gridLines = 4;
        p.setFont(QFont("Arial", 7));
        for (int i = 0; i <= gridLines; i++) {
            double val = maxVal * i / gridLines;
            int y = padT + chartH - (int)(chartH * i / gridLines);
            p.setPen(QPen(QColor("#f5d0e3"), 1, Qt::DashLine));
            p.drawLine(padL, y, padL + chartW, y);
            p.setPen(QColor("#9c2060"));
            p.drawText(0, y - 7, padL - 3, 14,
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number((int)val));
        }

        int n = data.size();
        QVector<QPointF> pts;
        for (int i = 0; i < n; i++) {
            double x = padL + (double)i / (n - 1) * chartW;
            double y = padT + chartH - (chartH * data[i].second / maxVal);
            pts.append(QPointF(x, y));
        }

        // Segment-by-segment reveal driven by m_progress (left → right)
        double totalSegs   = (double)(n - 1);
        double revealedSeg = m_progress * totalSegs;
        int    fullSegs    = (int)revealedSeg;
        double partFrac    = revealedSeg - fullSegs;

        // Build filled area polygon up to the revealed point
        QPolygonF area;
        area << QPointF(padL, padT + chartH);
        area << pts[0];
        for (int i = 0; i < fullSegs && i < n - 1; i++)
            area << pts[i + 1];
        if (fullSegs < n - 1) {
            QPointF mid = pts[fullSegs] + (pts[fullSegs + 1] - pts[fullSegs]) * partFrac;
            area << mid;
            area << QPointF(mid.x(), padT + chartH);
        } else {
            area << QPointF(padL + chartW, padT + chartH);
        }

        // Filled area — deep rose to transparent
        QLinearGradient areaGrad(0, padT, 0, padT + chartH);
        areaGrad.setColorAt(0, QColor(255, 77, 166, 90));   // #ff4da6 at 35% opacity
        areaGrad.setColorAt(1, QColor(194, 24, 91,  6));
        p.setPen(Qt::NoPen);
        p.setBrush(areaGrad);
        p.drawPolygon(area);

        // Draw revealed line segments — deep rose with slight purple cast
        p.setPen(QPen(QColor("#ff4da6"), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < fullSegs && i < n - 1; i++)
            p.drawLine(pts[i], pts[i + 1]);
        if (fullSegs < n - 1 && partFrac > 0) {
            QPointF mid = pts[fullSegs] + (pts[fullSegs + 1] - pts[fullSegs]) * partFrac;
            p.drawLine(pts[fullSegs], mid);
        }

        // Dots + value/percentage labels for revealed points
        int revealedDots = qMin(fullSegs + 1, n);
        QFont lblFont("Arial", 7);
        QFontMetrics fm(lblFont);
        int slotW = (n > 1) ? (chartW / (n - 1)) : chartW;

        for (int i = 0; i < revealedDots; i++) {
            // Soft glow ring
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(194, 24, 91, 45));
            p.drawEllipse(pts[i], 7, 7);
            // Dot — deep rose with white rim
            p.setBrush(QColor("#ff4da6"));
            p.setPen(QPen(QColor("white"), 1.5));
            p.drawEllipse(pts[i], 4, 4);

            // Value + percentage (appear once line is fully drawn)
            if (m_progress > 0.9) {
                int pct = (totalVal > 0) ? (int)(data[i].second / totalVal * 100.0) : 0;
                p.setFont(QFont("Arial", 7, QFont::Bold));
                p.setPen(QColor("#4a0026"));
                p.drawText((int)pts[i].x() - 18, (int)pts[i].y() - 22, 36, 12,
                           Qt::AlignCenter, QString::number((int)data[i].second));
                p.setPen(QColor("#9c2060"));
                p.drawText((int)pts[i].x() - 18, (int)pts[i].y() - 11, 36, 10,
                           Qt::AlignCenter, QString("%1%").arg(pct));
            }

            // X-axis label
            QString lbl = truncLabel(data[i].first, fm, slotW - 2);
            p.setFont(lblFont);
            p.setPen(QColor("#9c2060"));
            p.drawText((int)pts[i].x() - slotW/2, padT + chartH + 4,
                       slotW, 16, Qt::AlignHCenter | Qt::AlignTop, lbl);
        }

        // Axes — dusty rose
        p.setPen(QPen(QColor("#d4789c"), 1.5));
        p.drawLine(padL, padT, padL, padT + chartH);
        p.drawLine(padL, padT + chartH, padL + chartW, padT + chartH);
    }
};

// ============================================================
//  Constructor — wires up all UI and connects signals/slots
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupMenuBar();
    setupUI();
    setupStyles();
    addSampleData();
    updateSummaryCards();

    // --- Connect signals to slots ---
    connect(productTable, &QTableWidget::cellClicked,
            this, &MainWindow::selectProduct);

    connect(addButton,    &QPushButton::clicked,
            this, &MainWindow::addProduct);

    connect(editButton,   &QPushButton::clicked,
            this, &MainWindow::editProduct);

    connect(deleteButton, &QPushButton::clicked,
            this, &MainWindow::deleteProduct);

    connect(clearButton,  &QPushButton::clicked,
            this, &MainWindow::clearFields);

    connect(searchInput,  &QLineEdit::textChanged,
            this, &MainWindow::searchProducts);

    connect(tabWidget,    &QTabWidget::currentChanged,
            this, &MainWindow::onTabChanged);

    // Reapply status colors whenever selection changes
    connect(productTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        for (int r = 0; r < productTable->rowCount(); r++) {
            QTableWidgetItem *it = productTable->item(r, 7);
            if (it) applyStatusColor(r, it->text());
        }
    });

    // File menu actions
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveToCSV);
    connect(loadAction, &QAction::triggered, this, &MainWindow::loadFromCSV);

    // Initialize button states
    refreshButtonStates();
}

// ============================================================
//  Menu bar setup
// ============================================================
void MainWindow::setupMenuBar()
{
    menuBar_ = new QMenuBar(this);
    setMenuBar(menuBar_);

    fileMenu   = menuBar_->addMenu("File");
    saveAction = fileMenu->addAction("Save to CSV");
    loadAction = fileMenu->addAction("Load from CSV");
}

// ============================================================
//  Main UI — central widget with title, summary cards, tabs
// ============================================================
void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("BeautyVault: Smart Cosmetic Inventory System");
    resize(1360, 800);

    // App title label — left-aligned, feminine font
    titleLabel = new QLabel("✿  Beauty Vault");
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // --- Summary card factory lambda ---
    auto makeCard = [](const QString &title, QLabel *&valLabel) -> QFrame* {
        QFrame *card = new QFrame();
        card->setObjectName("summaryCard");
        QLabel *titleLbl = new QLabel(title);
        titleLbl->setObjectName("cardTitle");
        valLabel = new QLabel("0");
        valLabel->setObjectName("cardValue");
        QVBoxLayout *layout = new QVBoxLayout(card);
        layout->addWidget(titleLbl);
        layout->addWidget(valLabel);
        return card;
    };

    QFrame *card1 = makeCard("Total Products",  totalProductsValue);
    QFrame *card2 = makeCard("Inventory Value", totalValueValue);
    QFrame *card3 = makeCard("Low Stock Items", lowStockValue);

    QHBoxLayout *summaryLayout = new QHBoxLayout();
    summaryLayout->setSpacing(16);
    summaryLayout->addWidget(card1);
    summaryLayout->addWidget(card2);
    summaryLayout->addWidget(card3);

    // --- Tab widget ---
    tabWidget = new QTabWidget();
    tabWidget->addTab(buildInventoryTab(), "Inventory");
    tabWidget->addTab(buildDashboardTab(), "Dashboard");

    // --- Main layout ---
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 8, 12, 12);
    mainLayout->addWidget(titleLabel);
    mainLayout->addLayout(summaryLayout);
    mainLayout->addWidget(tabWidget);
}

// ============================================================
//  Tab 1: Inventory — form + table
// ============================================================
QWidget* MainWindow::buildInventoryTab()
{
    QWidget *tab = new QWidget();

    // --- Form labels ---
    productIdLabel   = new QLabel("Product ID:");
    productNameLabel = new QLabel("Product Name:");
    categoryLabel    = new QLabel("Category:");
    brandLabel       = new QLabel("Brand:");
    priceLabel       = new QLabel("Price:");
    stockLabel       = new QLabel("Stock Quantity:");

    // --- Form input widgets ---
    productIdInput   = new QLineEdit();
    productIdInput->setPlaceholderText("e.g. 006");

    productNameInput = new QLineEdit();
    productNameInput->setPlaceholderText("e.g. Rose Blush");

    brandInput       = new QLineEdit();
    brandInput->setPlaceholderText("e.g. MAC Cosmetics");

    // Category combo box with preset values
    categoryCombo = new QComboBox();
    categoryCombo->setMaxVisibleItems(8);
    categoryCombo->addItems({"Lipstick", "Skincare", "Foundation", "Powder",
                             "Blush", "Mascara", "Eyeshadow", "Concealer",
                             "Highlighter", "Bronzer"});

    // Spin boxes for numeric inputs
    stockSpinBox = new QSpinBox();
    stockSpinBox->setRange(0, 9999);

    priceSpinBox = new QDoubleSpinBox();
    priceSpinBox->setRange(0, 999999);
    priceSpinBox->setPrefix("PHP ");
    priceSpinBox->setDecimals(2);

    // --- Buttons ---
    addButton    = new QPushButton("Add Product");
    editButton   = new QPushButton("Save Edit");
    deleteButton = new QPushButton("Delete Product");
    clearButton  = new QPushButton("Clear Fields");

    addButton->setMinimumHeight(32);
    editButton->setMinimumHeight(32);
    deleteButton->setMinimumHeight(32);
    clearButton->setMinimumHeight(32);

    // Tooltips clarify each button's purpose
    addButton->setToolTip("Add a new product to the inventory");
    editButton->setToolTip("Save changes to the selected product");
    deleteButton->setToolTip("Delete the selected product");
    clearButton->setToolTip("Clear all form fields and deselect");

    // --- Status label that shows current mode ---
    QLabel *modeHint = new QLabel("Select a row to edit, or fill fields to add.");
    modeHint->setObjectName("modeHint");
    modeHint->setAlignment(Qt::AlignCenter);

    // --- Form grid layout ---
    QGridLayout *formLayout = new QGridLayout();
    formLayout->setVerticalSpacing(6);
    formLayout->addWidget(productIdLabel,   0, 0); formLayout->addWidget(productIdInput,   0, 1);
    formLayout->addWidget(productNameLabel, 1, 0); formLayout->addWidget(productNameInput, 1, 1);
    formLayout->addWidget(categoryLabel,    2, 0); formLayout->addWidget(categoryCombo,    2, 1);
    formLayout->addWidget(brandLabel,       3, 0); formLayout->addWidget(brandInput,       3, 1);
    formLayout->addWidget(priceLabel,       4, 0); formLayout->addWidget(priceSpinBox,     4, 1);
    formLayout->addWidget(stockLabel,       5, 0); formLayout->addWidget(stockSpinBox,     5, 1);
    formLayout->addWidget(modeHint,         6, 0, 1, 2);  // spans both columns
    formLayout->addWidget(addButton,        7, 0); formLayout->addWidget(editButton,       7, 1);
    formLayout->addWidget(deleteButton,     8, 0); formLayout->addWidget(clearButton,      8, 1);

    QGroupBox *formGroup = new QGroupBox("Product Information");
    formGroup->setLayout(formLayout);
    formGroup->setMaximumWidth(300);

    // --- Search bar ---
    searchInput = new QLineEdit();
    searchInput->setPlaceholderText("Search products by any field...");
    searchInput->setMinimumHeight(32);

    // --- Product table ---
    productTable = new QTableWidget();
    setupTable(tab);

    // --- Right panel: search + table ---
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(8);
    rightLayout->addWidget(searchInput);
    rightLayout->addWidget(productTable);

    // --- Main horizontal layout ---
    QHBoxLayout *mainLayout = new QHBoxLayout(tab);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->addWidget(formGroup, 1);
    mainLayout->addLayout(rightLayout, 2);

    return tab;
}

// ============================================================
//  Tab 2: Dashboard — stat cards + bar/line charts
// ============================================================
QWidget* MainWindow::buildDashboardTab()
{
    dashboardTab = new QWidget();

    // --- Dashboard stat card factory ---
    auto makeDashCard = [](const QString &label, QLabel *&valLabel,
                           const QString &color) -> QFrame* {
        QFrame *frame = new QFrame();
        frame->setObjectName("dashCard");
        frame->setStyleSheet(QString(
                                 "QFrame#dashCard { background: white; border: 2px solid %1;"
                                 " border-radius: 14px; padding: 12px; }"
                                 ).arg(color));

        QLabel *titleLbl = new QLabel(label);
        titleLbl->setStyleSheet(QString(
                                    "color:%1; font-weight:bold; font-size:12px;").arg(color));
        titleLbl->setAlignment(Qt::AlignCenter);

        valLabel = new QLabel("0");
        valLabel->setStyleSheet(QString(
                                    "color:%1; font-size:26px; font-weight:bold;").arg(color));
        valLabel->setAlignment(Qt::AlignCenter);

        QVBoxLayout *layout = new QVBoxLayout(frame);
        layout->addWidget(titleLbl);
        layout->addWidget(valLabel);
        return frame;
    };

    // Five stat cards with complementary colors
    QFrame *dc1 = makeDashCard("Products",     dashTotalProducts, "#ff4da6");  // bright pink
    QFrame *dc2 = makeDashCard("Value",        dashTotalValue,    "#FF75BB");  // light pink
    QFrame *dc3 = makeDashCard("Low Stock",    dashLowStock,      "#ff4da6");  // bright pink
    QFrame *dc4 = makeDashCard("Available",    dashAvailable,     "#FF75BB");  // light pink
    QFrame *dc5 = makeDashCard("Out of Stock", dashOutOfStock,    "#cc0066");  // deep pink

    // Store cards for targeted animation (avoids animating nested children)
    dashCards = {dc1, dc2, dc3, dc4, dc5};

    QHBoxLayout *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(12);
    cardsRow->addWidget(dc1);
    cardsRow->addWidget(dc2);
    cardsRow->addWidget(dc3);
    cardsRow->addWidget(dc4);
    cardsRow->addWidget(dc5);

    // --- Bar charts row ---
    stockChart    = new BarChartWidget("Stock per Product");
    stockChart->setColorScheme("purple");          // purple — distinct from category chart
    categoryChart = new BarChartWidget("Products per Category");

    QHBoxLayout *chartsRow = new QHBoxLayout();
    chartsRow->setSpacing(12);
    chartsRow->addWidget(stockChart,    1);
    chartsRow->addWidget(categoryChart, 1);

    // --- Line chart (value trend) ---
    timelineChart = new LineChartWidget("Inventory Value by Product");

    // Store chart widgets for targeted animation
    dashChartWidgets = {stockChart, categoryChart, timelineChart};

    // --- Dashboard tab layout ---
    QVBoxLayout *layout = new QVBoxLayout(dashboardTab);
    layout->setSpacing(14);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->addLayout(cardsRow);
    layout->addLayout(chartsRow, 1);
    layout->addWidget(timelineChart, 1);

    return dashboardTab;
}

// ============================================================
//  Table setup — columns, headers, sizing, delegate
// ============================================================
void MainWindow::setupTable(QWidget *)
{
    productTable->setColumnCount(8);
    productTable->setHorizontalHeaderLabels(
        {"#", "ID", "Name", "Category", "Brand", "Price", "Stock", "Status"});

    productTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    productTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    productTable->setColumnWidth(0, 36);

    productTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    productTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productTable->verticalHeader()->setVisible(false);
    productTable->setShowGrid(true);
    productTable->setGridStyle(Qt::SolidLine);

    // Custom delegate for the Status column to preserve color on selection
    productTable->setItemDelegateForColumn(7, new StatusDelegate(productTable));
}

// ============================================================
//  Stylesheet — pink primary + teal/gold/violet complements
// ============================================================
void MainWindow::setupStyles()
{
    setStyleSheet(
        "QMainWindow { background-color: #ffe0f0; }"
        "QMenuBar { background-color: #ff4da6; color: white; font-weight: bold; }"
        "QMenuBar::item:selected { background-color: #cc0066; }"
        "QMenu { background-color: #fff0f8; color: #cc0066; border: 1px solid #ff4da6; }"
        "QMenu::item:selected { background-color: #ff4da6; color: white; }"
        "QLabel { color: #cc0066; font-size: 12px; }"
        // Summary cards
        "QFrame#summaryCard { background-color: white; border: 2px solid #ff4da6;"
        "  border-radius: 12px; padding: 8px; }"
        "QLabel#cardTitle { color: #cc0066; font-weight: bold; font-size: 11px; }"
        "QLabel#cardValue { color: #ff4da6; font-size: 20px; font-weight: bold; }"
        // Mode hint label
        "QLabel#modeHint { color: #ff4da6; font-size: 10px; font-style: italic; }"
        // Tabs
        "QTabWidget::pane { border: 2px solid #ff4da6; border-radius: 8px; background: #ffe0f0; }"
        "QTabBar::tab { background: #FF75BB; color: white; font-weight: bold; font-size: 12px;"
        "  padding: 7px 20px; border-radius: 6px 6px 0 0; margin-right: 3px; }"
        "QTabBar::tab:selected { background: #ff4da6; color: white; }"
        "QTabBar::tab:hover { background: #ff4da6; color: white; }"
        // Group box — bright pink
        "QGroupBox { font-weight: bold; font-size: 12px; color: #cc0066;"
        "  border: 2px solid #ff4da6; border-radius: 10px; margin-top: 12px; padding-top: 4px;"
        "  background-color: #fff0f8; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px;"
        "  color: #cc0066; background-color: #ffc0e0; border-radius: 4px; }"
        // Buttons — default (Add)
        "QPushButton { background-color: #ff4da6; color: white; font-weight: bold;"
        "  padding: 6px 10px; border-radius: 8px; font-size: 12px; }"
        "QPushButton:hover   { background-color: #e6008a; }"
        "QPushButton:pressed { background-color: #cc0066; }"
        "QPushButton:disabled { background-color: #ffb3d9; color: #ffffff; }"
        // Edit button — medium pink
        "QPushButton#editBtn { background-color: #FF75BB; color: white; }"
        "QPushButton#editBtn:hover { background-color: #ff4da6; }"
        "QPushButton#editBtn:disabled { background-color: #ffc0e0; color: white; }"
        // Delete button — deep pink
        "QPushButton#deleteBtn { background-color: #cc0066; color: white; }"
        "QPushButton#deleteBtn:hover { background-color: #99004d; }"
        "QPushButton#deleteBtn:disabled { background-color: #e080b0; color: white; }"
        // Clear button — hot pink
        "QPushButton#clearBtn { background-color: #ff4da6; color: white; }"
        "QPushButton#clearBtn:hover { background-color: #cc0066; }"
        // Input fields
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {"
        "  background-color: white; color: #222222;"
        "  border: 1px solid #ff4da6; border-radius: 5px; padding: 3px 5px; font-size: 12px; }"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {"
        "  border: 2px solid #cc0066; }"
        "QComboBox QAbstractItemView {"
        "  background-color: white; color: #222222;"
        "  border: 2px solid #ff4da6; border-radius: 6px;"
        "  selection-background-color: #ff4da6; selection-color: white; padding: 2px; }"
        "QComboBox QAbstractItemView::item { min-height: 22px; padding: 2px 6px; color: #222222; }"
        // Product table
        "QTableWidget { background-color: white; border: 1px solid #ff4da6;"
        "  gridline-color: #ffc0e0; font-size: 12px; }"
        "QTableWidget::item { color: #222222; padding: 3px; }"
        "QTableWidget::item:selected { color: white; background-color: #ff4da6; }"
        "QTableWidget QTableCornerButton::section { background-color: #ff4da6; }"
        "QHeaderView::section { background-color: #ff4da6; color: white;"
        "  font-weight: bold; border: 1px solid #e6008a; padding: 4px; font-size: 12px; }"
        );

    // Title bar — feminine font, left-aligned
    titleLabel->setStyleSheet(
        "font-size: 26px; font-weight: bold; color: #cc0066;"
        "font-family: 'Georgia', 'Palatino Linotype', serif;"
        "font-style: italic;"
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #ffc0e0, stop:0.6 #ffe0f0, stop:1 #ffe0f0);"
        "border-radius: 10px; padding: 6px 16px;"
        "letter-spacing: 1px;"
        );

    // Assign object names for per-button styles
    editButton->setObjectName("editBtn");
    deleteButton->setObjectName("deleteBtn");
    clearButton->setObjectName("clearBtn");
}

// ============================================================
//  Dashboard: no fade-in animation (removed to prevent flickering)
//  Charts and cards render immediately when tab is opened.
// ============================================================
void MainWindow::animateDashboardIn()
{
    // Animation removed — was causing flickering on repeated tab visits.
    // All dashboard widgets are already visible; nothing to do here.
}

// ============================================================
//  Sample data — pre-loads 5 products on startup
// ============================================================
void MainWindow::addSampleData()
{
    // Define sample products using the Product struct
    QList<Product> sampleProducts = {
                                     {"001", "Matte Lipstick",       "Lipstick",   "Maybelline",    499.00,  20, ""},
                                     {"002", "Vitamin C Serum",      "Skincare",   "The Ordinary",  850.00,   5, ""},
                                     {"003", "Liquid Foundation",    "Foundation", "Fenty Beauty", 2200.00,   0, ""},
                                     {"004", "Loose Setting Powder", "Powder",     "Laura Mercier", 1800.00, 12, ""},
                                     {"005", "Rose Blush",           "Blush",      "NARS",          1500.00,  3, ""},
                                     };

    for (Product &prod : sampleProducts) {
        prod.status = getStockStatus(prod.stock);  // compute status from stock

        int row = productTable->rowCount();
        productTable->insertRow(row);
        productTable->setItem(row, 1, makeItem(prod.id));
        productTable->setItem(row, 2, makeItem(prod.name));
        productTable->setItem(row, 3, makeItem(prod.category));
        productTable->setItem(row, 4, makeItem(prod.brand));
        productTable->setItem(row, 5, makeItem(QString::number(prod.price, 'f', 2)));
        productTable->setItem(row, 6, makeItem(QString::number(prod.stock)));
        productTable->setItem(row, 7, makeItem(prod.status));
        applyStatusColor(row, prod.status);

        products.append(prod);  // also store in QVector
    }
    updateRowNumbers();
}

// ============================================================
//  Update top summary cards (total, value, low stock)
// ============================================================
void MainWindow::updateSummaryCards()
{
    int total = productTable->rowCount();
    double value = 0;
    int low = 0;

    for (int r = 0; r < total; r++) {
        QTableWidgetItem *priceItem = productTable->item(r, 5);
        QTableWidgetItem *stockItem = productTable->item(r, 6);
        if (!priceItem || !stockItem) continue;

        value += priceItem->text().toDouble() * stockItem->text().toInt();
        if (stockItem->text().toInt() <= 5) low++;
    }

    totalProductsValue->setText(QString::number(total));
    totalValueValue->setText("PHP " + QString::number(value, 'f', 2));
    lowStockValue->setText(QString::number(low));
}

// ============================================================
//  Update dashboard charts and stat cards
// ============================================================
void MainWindow::updateDashboard()
{
    int total = 0, low = 0, avail = 0, outOf = 0;
    double value = 0;
    QMap<QString, int> catCount;
    QList<QPair<QString, double>> stockData;
    QList<QPair<QString, double>> valueData;

    for (int r = 0; r < productTable->rowCount(); r++) {
        QTableWidgetItem *nameItem  = productTable->item(r, 2);
        QTableWidgetItem *catItem   = productTable->item(r, 3);
        QTableWidgetItem *priceItem = productTable->item(r, 5);
        QTableWidgetItem *stockItem = productTable->item(r, 6);
        QTableWidgetItem *statItem  = productTable->item(r, 7);
        if (!stockItem) continue;

        int    stock  = stockItem->text().toInt();
        double price  = priceItem ? priceItem->text().toDouble() : 0;
        QString name  = nameItem  ? nameItem->text()  : "";
        QString cat   = catItem   ? catItem->text()   : "Other";
        QString stat  = statItem  ? statItem->text()  : "";

        total++;
        value += price * stock;

        // Tally by status
        if (stat == "Low Stock")    low++;
        if (stat == "Available")    avail++;
        if (stat == "Out of Stock") outOf++;

        // Aggregate by category
        catCount[cat]++;

        // Shorten name for chart label
        QString shortName = name.length() > 10 ? name.left(9) + "..." : name;
        stockData.append({shortName, (double)stock});
        valueData.append({shortName, price * stock});
    }

    // Update dashboard stat labels
    dashTotalProducts->setText(QString::number(total));
    dashTotalValue->setText("PHP " + QString::number(value, 'f', 0));
    dashLowStock->setText(QString::number(low));
    dashAvailable->setText(QString::number(avail));
    dashOutOfStock->setText(QString::number(outOf));

    // Push data to charts
    stockChart->setData(stockData);

    QList<QPair<QString, double>> catData;
    for (auto it = catCount.begin(); it != catCount.end(); ++it)
        catData.append({it.key(), (double)it.value()});
    categoryChart->setData(catData);

    timelineChart->setData(valueData);
}

// ============================================================
//  Tab changed — refresh dashboard on switch
// ============================================================
void MainWindow::onTabChanged(int index)
{
    if (index == 1) {
        updateDashboard();
        // Small delay so the tab is fully visible before animating
        QTimer::singleShot(80, this, &MainWindow::animateDashboardIn);
    }
}

// ============================================================
//  CRUD slot: Add Product
// ============================================================
void MainWindow::addProduct()
{
    // Validate required fields
    QString id    = productIdInput->text().trimmed();
    QString name  = productNameInput->text().trimmed();
    QString brand = brandInput->text().trimmed();

    if (id.isEmpty() || name.isEmpty() || brand.isEmpty()) {
        QMessageBox::warning(this, "Missing Fields",
                             "Please fill in Product ID, Name, and Brand.");
        return;
    }

    // Check for duplicate product ID
    for (int r = 0; r < productTable->rowCount(); r++) {
        QTableWidgetItem *idItem = productTable->item(r, 1);
        if (idItem && idItem->text() == id) {
            QMessageBox::warning(this, "Duplicate ID",
                                 "ID \"" + id + "\" already exists.");
            return;
        }
    }

    // Gather remaining field values
    int    stock  = stockSpinBox->value();
    double price  = priceSpinBox->value();
    QString status = getStockStatus(stock);

    // Build a Product struct and append to the backend vector
    Product prod;
    prod.id       = id;
    prod.name     = name;
    prod.category = categoryCombo->currentText();
    prod.brand    = brand;
    prod.price    = price;
    prod.stock    = stock;
    prod.status   = status;
    products.append(prod);

    // Insert new row into the table using the struct's fields
    int row = productTable->rowCount();
    productTable->insertRow(row);
    productTable->setItem(row, 1, makeItem(prod.id));
    productTable->setItem(row, 2, makeItem(prod.name));
    productTable->setItem(row, 3, makeItem(prod.category));
    productTable->setItem(row, 4, makeItem(prod.brand));
    productTable->setItem(row, 5, makeItem(QString::number(prod.price, 'f', 2)));
    productTable->setItem(row, 6, makeItem(QString::number(prod.stock)));
    productTable->setItem(row, 7, makeItem(prod.status));
    applyStatusColor(row, prod.status);

    updateRowNumbers();
    updateSummaryCards();
    clearFields();

    QMessageBox::information(this, "Product Added",
                             "\"" + name + "\" has been added to inventory.");
}

// ============================================================
//  CRUD slot: Edit Product (Save Edit)
// ============================================================
void MainWindow::editProduct()
{
    if (editingRow < 0) return;

    // Validate required fields
    QString id    = productIdInput->text().trimmed();
    QString name  = productNameInput->text().trimmed();
    QString brand = brandInput->text().trimmed();

    if (id.isEmpty() || name.isEmpty() || brand.isEmpty()) {
        QMessageBox::warning(this, "Missing Fields",
                             "Please fill in Product ID, Name, and Brand.");
        return;
    }

    // Apply updated values to the existing row
    int    stock  = stockSpinBox->value();
    double price  = priceSpinBox->value();
    QString status = getStockStatus(stock);

    // Sync backend vector — update the struct at the matching index
    if (editingRow >= 0 && editingRow < products.size()) {
        products[editingRow].id       = id;
        products[editingRow].name     = name;
        products[editingRow].category = categoryCombo->currentText();
        products[editingRow].brand    = brand;
        products[editingRow].price    = price;
        products[editingRow].stock    = stock;
        products[editingRow].status   = status;
    }

    // Reflect updated struct fields back into the table
    productTable->setItem(editingRow, 1, makeItem(id));
    productTable->setItem(editingRow, 2, makeItem(name));
    productTable->setItem(editingRow, 3, makeItem(categoryCombo->currentText()));
    productTable->setItem(editingRow, 4, makeItem(brand));
    productTable->setItem(editingRow, 5, makeItem(QString::number(price, 'f', 2)));
    productTable->setItem(editingRow, 6, makeItem(QString::number(stock)));
    productTable->setItem(editingRow, 7, makeItem(status));
    applyStatusColor(editingRow, status);

    updateSummaryCards();
    setEditMode(false);
    clearFields();

    QMessageBox::information(this, "Product Updated",
                             "\"" + name + "\" has been updated.");
}

// ============================================================
//  CRUD slot: Delete Product
// ============================================================
void MainWindow::deleteProduct()
{
    int row = productTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection",
                             "Please select a row to delete.");
        return;
    }

    QString name = productTable->item(row, 2)
                       ? productTable->item(row, 2)->text()
                       : "this product";

    // Confirm deletion with a Yes/No dialog (QMessageBox used here)
    int reply = QMessageBox::question(this, "Confirm Delete",
                                      "Delete \"" + name + "\"?",
                                      QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Remove from backend vector first, then from the table
        if (row >= 0 && row < products.size())
            products.removeAt(row);

        productTable->removeRow(row);
        updateRowNumbers();
        updateSummaryCards();
        setEditMode(false);
        clearFields();
    }
}

// ============================================================
//  CRUD slot: Clear Fields
// ============================================================
void MainWindow::clearFields()
{
    productIdInput->clear();
    productNameInput->clear();
    brandInput->clear();
    categoryCombo->setCurrentIndex(0);
    priceSpinBox->setValue(0);
    stockSpinBox->setValue(0);
    productTable->clearSelection();
    setEditMode(false);
}

// ============================================================
//  Search slot: filter table rows in real time
// ============================================================
void MainWindow::searchProducts(const QString &text)
{
    for (int r = 0; r < productTable->rowCount(); r++) {
        bool match = false;
        // Check all columns (skip column 0 which is just the row number)
        for (int c = 1; c < productTable->columnCount(); c++) {
            QTableWidgetItem *item = productTable->item(r, c);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        productTable->setRowHidden(r, !match);
    }
}

// ============================================================
//  Slot: row clicked — fill form and enter edit mode
// ============================================================
void MainWindow::selectProduct(int row, int)
{
    // Helper lambda to safely read a cell's text
    auto get = [&](int col) -> QString {
        QTableWidgetItem *item = productTable->item(row, col);
        return item ? item->text() : "";
    };

    productIdInput->setText(get(1));
    productNameInput->setText(get(2));
    categoryCombo->setCurrentText(get(3));
    brandInput->setText(get(4));
    priceSpinBox->setValue(get(5).toDouble());
    stockSpinBox->setValue(get(6).toInt());

    // Switch to edit mode: track which row is being edited
    editingRow = row;
    productIdInput->setReadOnly(true);   // prevent ID changes during edit
    setEditMode(true);
}

// ============================================================
//  setEditMode — switches between Add mode and Edit mode.
//  Both Add and Save Edit buttons are always visible;
//  only their enabled state and the group box title change.
// ============================================================
void MainWindow::setEditMode(bool editing)
{
    if (editing) {
        // Edit mode: disable Add, enable Save Edit
        addButton->setEnabled(false);
        editButton->setEnabled(true);

        // Update group box title to make mode obvious
        QGroupBox *gb = qobject_cast<QGroupBox*>(addButton->parentWidget()->parentWidget());
        if (gb) gb->setTitle("Product Information  [EDITING ROW " + QString::number(editingRow + 1) + "]");

    } else {
        // Add mode: enable Add, disable Save Edit
        editingRow = -1;
        productIdInput->setReadOnly(false);
        addButton->setEnabled(true);
        editButton->setEnabled(false);

        QGroupBox *gb = qobject_cast<QGroupBox*>(addButton->parentWidget()->parentWidget());
        if (gb) gb->setTitle("Product Information");
    }

    refreshButtonStates();
}

// ============================================================
//  refreshButtonStates — ensures button enabled/disabled states
//  are always consistent with the current mode
// ============================================================
void MainWindow::refreshButtonStates()
{
    bool isEditing = (editingRow >= 0);
    bool hasSelection = (productTable->currentRow() >= 0);

    addButton->setEnabled(!isEditing);
    editButton->setEnabled(isEditing);
    deleteButton->setEnabled(hasSelection);
}

// ============================================================
//  Helpers
// ============================================================

// Create a standard table cell item with dark text
QTableWidgetItem* MainWindow::makeItem(const QString &text)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setForeground(QColor("#222222"));
    item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    return item;
}

// Compute stock status string from quantity
QString MainWindow::getStockStatus(int stock)
{
    if (stock == 0)  return "Out of Stock";
    if (stock <= 5)  return "Low Stock";
    return "Available";
}

// Apply color-coded background to the Status cell
void MainWindow::applyStatusColor(int row, const QString &status)
{
    QTableWidgetItem *item = productTable->item(row, 7);
    if (!item) return;

    item->setTextAlignment(Qt::AlignCenter);
    item->setFont(QFont("", -1, QFont::Bold));

    QColor bg, fg("#ffffff");
    if      (status == "Available")   bg = QColor("#ff4da6");  // bright pink
    else if (status == "Low Stock")   bg = QColor("#FF75BB");  // light pink
    else                              bg = QColor("#cc0066");  // deep pink (out of stock)

    item->setData(Qt::BackgroundRole, bg);
    item->setData(Qt::ForegroundRole, fg);
}

// Renumber the first column after any insert/delete
void MainWindow::updateRowNumbers()
{
    for (int row = 0; row < productTable->rowCount(); row++) {
        QTableWidgetItem *numItem = new QTableWidgetItem(QString::number(row + 1));
        numItem->setTextAlignment(Qt::AlignCenter);
        numItem->setForeground(QColor("#ff4da6"));
        numItem->setFont(QFont("", -1, QFont::Bold));
        productTable->setItem(row, 0, numItem);
    }
}

// ============================================================
//  Save inventory to CSV file
// ============================================================
void MainWindow::saveToCSV()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Inventory",
                                                "BeautyVault_Inventory.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot open file for writing.");
        return;
    }

    QTextStream out(&file);
    out << "ID,Name,Category,Brand,Price,Stock,Status\n";

    // Sync backend vector from table, then write from the struct vector
    products.clear();
    for (int r = 0; r < productTable->rowCount(); r++) {
        auto get = [&](int c) -> QString {
            QTableWidgetItem *it = productTable->item(r, c);
            return it ? it->text() : "";
        };
        Product p;
        p.id       = get(1);
        p.name     = get(2);
        p.category = get(3);
        p.brand    = get(4);
        p.price    = get(5).toDouble();
        p.stock    = get(6).toInt();
        p.status   = get(7);
        products.append(p);
    }

    // Write each Product struct to the CSV
    for (const Product &p : products) {
        auto escape = [](const QString &s) -> QString {
            return s.contains(',') ? "\"" + s + "\"" : s;
        };
        out << escape(p.id)       << ","
            << escape(p.name)     << ","
            << escape(p.category) << ","
            << escape(p.brand)    << ","
            << QString::number(p.price, 'f', 2) << ","
            << p.stock            << ","
            << escape(p.status)   << "\n";
    }

    file.close();
    QMessageBox::information(this, "Saved", "Inventory saved to:\n" + path);
}

// ============================================================
//  Load inventory from CSV file
// ============================================================
void MainWindow::loadFromCSV()
{
    QString path = QFileDialog::getOpenFileName(this, "Load Inventory",
                                                "", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot open file for reading.");
        return;
    }

    int reply = QMessageBox::question(this, "Load CSV",
                                      "Replace all current data with the file's contents?",
                                      QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    productTable->setRowCount(0);
    products.clear();  // reset backend vector before repopulating

    QTextStream in(&file);
    bool firstLine = true;

    // Pass 1: parse each CSV line into a Product struct, append to vector
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (firstLine) { firstLine = false; continue; }  // skip header
        if (line.isEmpty()) continue;

        QStringList cols = line.split(",");
        if (cols.size() < 6) continue;
        for (QString &s : cols) s = s.remove('"').trimmed();

        Product p;
        p.id       = cols[0];
        p.name     = cols[1];
        p.category = cols[2];
        p.brand    = cols[3];
        p.price    = cols[4].toDouble();
        p.stock    = cols[5].toInt();
        p.status   = (cols.size() >= 7) ? cols[6] : getStockStatus(p.stock);
        products.append(p);
    }
    file.close();

    // Pass 2: populate the table from the backend struct vector
    for (const Product &p : products) {
        int row = productTable->rowCount();
        productTable->insertRow(row);
        productTable->setItem(row, 1, makeItem(p.id));
        productTable->setItem(row, 2, makeItem(p.name));
        productTable->setItem(row, 3, makeItem(p.category));
        productTable->setItem(row, 4, makeItem(p.brand));
        productTable->setItem(row, 5, makeItem(QString::number(p.price, 'f', 2)));
        productTable->setItem(row, 6, makeItem(QString::number(p.stock)));
        productTable->setItem(row, 7, makeItem(p.status));
        applyStatusColor(row, p.status);
    }

    updateRowNumbers();
    updateSummaryCards();

    QMessageBox::information(this, "Loaded",
                             QString::number(productTable->rowCount()) + " products loaded from CSV.");
}

// Required because BarChartWidget and LineChartWidget use Q_OBJECT
// inside the .cpp file — MOC must be included manually here.
#include "mainwindow.moc"


