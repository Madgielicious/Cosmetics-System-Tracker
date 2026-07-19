#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFrame>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>

// Forward declarations for custom chart widgets defined in mainwindow.cpp
class BarChartWidget;
class LineChartWidget;

// ============================================================
//  Product struct — stores one inventory record (7 fields)
// ============================================================
struct Product {
    QString id;
    QString name;
    QString category;
    QString brand;
    double  price;
    int     stock;
    QString status;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT   // Required for signals/slots

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    // --- Inventory CRUD slots ---
    void selectProduct(int row, int column);  // fill form on row click
    void addProduct();                         // insert new product
    void editProduct();                        // save edited product
    void deleteProduct();                      // remove selected row
    void clearFields();                        // reset form fields
    void searchProducts(const QString &text); // real-time search filter

    // --- File I/O slots ---
    void saveToCSV();
    void loadFromCSV();

    // --- Navigation slots ---
    void onTabChanged(int index);

private:
    // ── Menu bar ──────────────────────────────────────────
    QMenuBar  *menuBar_;
    QMenu     *fileMenu;
    QAction   *saveAction;
    QAction   *loadAction;

    // ── Tab widget ────────────────────────────────────────
    QTabWidget *tabWidget;

    // ── Summary cards (top bar) ───────────────────────────
    QLabel *titleLabel;
    QLabel *totalProductsValue;
    QLabel *totalValueValue;
    QLabel *lowStockValue;

    // ── Form labels (Inventory tab) ───────────────────────
    QLabel *productIdLabel;
    QLabel *productNameLabel;
    QLabel *categoryLabel;
    QLabel *brandLabel;
    QLabel *priceLabel;
    QLabel *stockLabel;

    // ── Form input widgets ────────────────────────────────
    QLineEdit      *productIdInput;    // QLineEdit
    QLineEdit      *productNameInput;  // QLineEdit
    QLineEdit      *brandInput;        // QLineEdit
    QLineEdit      *searchInput;       // QLineEdit — search
    QComboBox      *categoryCombo;     // QComboBox
    QSpinBox       *stockSpinBox;      // QSpinBox
    QDoubleSpinBox *priceSpinBox;      // QDoubleSpinBox

    // ── Action buttons ────────────────────────────────────
    QPushButton *addButton;    // QPushButton
    QPushButton *editButton;   // QPushButton
    QPushButton *deleteButton; // QPushButton
    QPushButton *clearButton;  // QPushButton

    // ── Product table and edit state ─────────────────────
    QTableWidget *productTable;  // QTableWidget
    int editingRow = -1;         // -1 = not editing; >= 0 = row being edited

    // ── Dashboard widgets ─────────────────────────────────
    BarChartWidget  *stockChart;
    BarChartWidget  *categoryChart;
    LineChartWidget *timelineChart;
    QLabel *dashTotalProducts;
    QLabel *dashTotalValue;
    QLabel *dashLowStock;
    QLabel *dashAvailable;
    QLabel *dashOutOfStock;

    // ── Dashboard animation helpers ───────────────────────
    QWidget *dashboardTab;
    // Store top-level animatable widgets (cards + charts) directly
    // so animateDashboardIn() never touches nested label children
    QList<QFrame*>  dashCards;       // the 5 stat card frames
    QList<QWidget*> dashChartWidgets; // stockChart, categoryChart, timelineChart
    void animateDashboardIn();

    // ── Product data store (QVector of Product structs) ───
    QVector<Product> products;

    // ── Private helper methods ────────────────────────────
    void setupMenuBar();
    void setupUI();
    QWidget* buildInventoryTab();
    QWidget* buildDashboardTab();
    void setupTable(QWidget *parent);
    void setupStyles();
    void addSampleData();
    void updateSummaryCards();
    void updateRowNumbers();
    void updateDashboard();
    void applyStatusColor(int row, const QString &status);
    QString getStockStatus(int stock);
    QTableWidgetItem *makeItem(const QString &text);
    void setEditMode(bool editing);
    void refreshButtonStates();
};

#endif // MAINWINDOW_H


