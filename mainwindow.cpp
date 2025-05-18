#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "main.cpp"
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/HTMLtree.h>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <QClipboard>
#include <QMimeData>
#include <QString>

bool flag=true;
QString myString = "Hello, World!";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                   Qt::WindowMinimizeButtonHint );// кнопка-вопросик тут должна появиться, но не появилась
    this->setAttribute(Qt::WA_CustomWhatsThis);
    this -> setTrayIconActions();
    this -> showTrayIcon();
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(bufferChanged()), Qt::QueuedConnection);
}


QString modifyHtmlFont(const QString& html, const QString& fontName, bool clearBackgroundColor = false) {
    // Преобразуем входную HTML-строку в QByteArray, используя кодировку UTF-8
    QByteArray htmlBytes = html.toUtf8();
    // Получаем указатель на данные QByteArray как const char* (для libxml2)
    const char* htmlStr = htmlBytes.constData();

    // Парсим HTML-документ из памяти с помощью libxml2
    // HTML_PARSE_RECOVER: Включает режим восстановления после ошибок
    htmlDocPtr doc = htmlReadMemory(htmlStr, htmlBytes.size(), nullptr, "UTF-8", HTML_PARSE_RECOVER);
    if (!doc) {
        qDebug() << "Ошибка парсинга HTML";
        return QString(); // Возвращаем пустую строку в случае ошибки
    }

    // Создаем XPath контекст для запросов к HTML-документу
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        qDebug() << "Ошибка создания XPath контекста";
        xmlFreeDoc(doc); // Освобождаем ресурсы, выделенные для документа
        return QString();
    }

    // Создаем XPath выражение для выбора ВСЕХ элементов в документе (xpathExpr = "//*")
    const xmlChar* xpathExpr = (const xmlChar*)"//*";
    // Выполняем XPath-запрос и получаем результат в виде xmlXPathObject
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if (!xpathObj) {
        qDebug() << "Ошибка выполнения XPath запроса";
        xmlFreeDoc(doc); // Освобождаем ресурсы, выделенные для документа
        xmlXPathFreeContext(xpathCtx); // Освобождаем ресурсы, выделенные для XPath контекста
        return QString();
    }

    // Получаем набор узлов, соответствующих XPath выражению
    xmlNodeSetPtr nodeset = xpathObj->nodesetval;
    if (nodeset) {
        // Итерируем по каждому узлу в наборе
        for (int i = 0; i < nodeset->nodeNr; ++i) {
            // Получаем указатель на текущий узел
            xmlNodePtr node = nodeset->nodeTab[i];

            // Получаем существующий атрибут "style"
            xmlAttrPtr styleAttr = xmlHasProp(node, (const xmlChar*)"style");

            QString existingStyle;

            if (styleAttr) {
                // Получаем значение существующего атрибута "style"
                xmlChar *styleValue = xmlGetProp(node, (const xmlChar*)"style");
                if (styleValue) {
                    existingStyle = QString::fromUtf8((const char*)styleValue);
                    xmlFree(styleValue);  // Освобождаем память, выделенную для значения
                }

                xmlUnlinkNode((xmlNodePtr)styleAttr); // Отсоединяем атрибут от узла
                xmlFreeNode((xmlNodePtr)styleAttr);  // Освобождаем память, выделенную для атрибута
            }

            // Добавляем или изменяем "font-family" в существующих стилях
            QString newStyle;
            if (existingStyle.isEmpty()) {
                newStyle = QString("font-family: %1;").arg(fontName);
            } else {
                newStyle = existingStyle;

                // Убираем background-color (если нужно)
                if (clearBackgroundColor) {
                    int startIndex = newStyle.indexOf("background-color:");
                    if (startIndex != -1) {
                        int endIndex = newStyle.indexOf(";", startIndex);
                        if (endIndex == -1) {
                            endIndex = newStyle.length();
                        }
                        newStyle.replace(startIndex, endIndex - startIndex, "");
                        newStyle = newStyle.simplified(); // Убираем лишние пробелы
                    }
                }

                // Проверяем, есть ли уже "font-family" в стилях
                if (newStyle.contains("font-family:")) {
                    // Заменяем существующее значение
                    int startIndex = newStyle.indexOf("font-family:");
                    int endIndex = newStyle.indexOf(";", startIndex);
                    if (endIndex == -1) {
                        endIndex = newStyle.length();
                    }
                    newStyle.replace(startIndex, endIndex - startIndex, QString("font-family: %1").arg(fontName));
                } else {
                    // Добавляем "font-family" к существующим стилям
                    newStyle += QString(" font-family: %1;").arg(fontName);
                }
            }

            // Формируем значение для атрибута "style" с новым набором стилей
            QByteArray styleValue = newStyle.toUtf8();
            // Добавляем или заменяем атрибут "style" у текущего узла с новым значением
            xmlNewProp(node, (const xmlChar*)"style", (const xmlChar*)styleValue.constData());
        }
    }

    // Освобождаем ресурсы
    xmlXPathFreeObject(xpathObj);
    // Освобождаем ресурсы
    xmlXPathFreeContext(xpathCtx);

    // Сериализуем измененный HTML-документ обратно в строку
    xmlChar* xmlbuff;
    int buffersize;
    htmlDocDumpMemory(doc, &xmlbuff, &buffersize);

    // Преобразуем результат из char* в QString, используя кодировку UTF-8
    QString result = QString::fromUtf8((const char*)xmlbuff, buffersize);
    // Освобождаем ресурсы
    xmlFree(xmlbuff);
    // Освобождаем ресурсы
    xmlFreeDoc(doc);

    // Возвращаем измененную HTML-строку
    return result;
}
void MainWindow::bufferChanged()
{
    {
        //Сравниваем текст в буфере с предыдущим значением, чтобы избежать бесконечной рекурсии
        if (myString != clipboard->text())
        {
            qDebug() << "bufferChanged() called"; //Выводим сообщение в отладочный вывод
            //Получаем данные из буфера обмена в виде QMimeData
            const QMimeData *mimeData = clipboard->mimeData();
            QString html; //Объявляем переменную для хранения нового HTML-содержимого
            //Создаем новые объекты QTextEdit и QMimeData (временные объекты)
            QTextEdit *textEdit = new QTextEdit();
            QMimeData *newMimeData = new QMimeData();
            //Проверяем, есть ли в буфере обмена данные в формате text/html
            if (mimeData->hasFormat("text/html"))
            {
                //Изменяем шрифт исходной HTML-строки, используя функцию modifyHtmlFont
                QString modifiedHtml = modifyHtmlFont(mimeData->html(), "Times New Roman",1);
                //Устанавливаем модифицированный HTML в textEdit
                textEdit->setHtml(modifiedHtml);
                //Создаем новый QMimeData с измененным HTML
                newMimeData->setHtml(textEdit->toHtml());
                newMimeData->setText(textEdit->toPlainText());

                //Получаем HTML из нового QMimeData (для дальнейшего использования)
                html = newMimeData->html();

                //Блокируем сигналы буфера обмена, чтобы избежать рекурсивного вызова bufferChanged()
                clipboard->blockSignals(true);
                //Устанавливаем новый QMimeData в буфер обмена
                clipboard->setMimeData(newMimeData, QClipboard::Clipboard);
                //Разблокируем сигналы буфера обмена
                clipboard->blockSignals(false);
            }
            //Если в буфере обмена есть только текст (без HTML)
            else if (mimeData->hasText())
            {
                //Получаем простой текст из буфера обмена
                QString plainText = mimeData->text();
                //Создаем новый HTML-код, оборачивая текст в тег <span> с указанным шрифтом
                QString newHtml = QString("<span style=\"font-family: Times New Roman; font-size: 12pt;\">%1</span>").arg(plainText);
                textEdit->setHtml(newHtml);
                newMimeData->setHtml(textEdit->toHtml());
                newMimeData->setText(textEdit->toPlainText());
                html = newMimeData->html();
                //Устанавливаем новый QMimeData в буфер обмена
                clipboard->blockSignals(true);
                clipboard->setMimeData(newMimeData);
                clipboard->blockSignals(false);
                qDebug() << "Текст скопирован в буфер с новым шрифтом (в виде HTML)";
            }
            qDebug() << "bufferChanged() finished"; //Выводим сообщение об окончании работы функции
            //Добавляем содержимое в QTextEdit (ui->textEdit)
            ui->textEdit->append(html);
            //qDebug() << html();
            //Обновляем myString, чтобы избежать повторной обработки того же содержимого
            myString = clipboard->text();
        }
        else
        {
            return; //Если текст в буфере не изменился, выходим из функции
        }
    }
}

void MainWindow::showTrayIcon()
{
    //Создаём экземпляр класса и задаём его свойства
    trayIcon = new QSystemTrayIcon(this);
    QIcon trayImage(":/images/logo.png");
    trayIcon -> setIcon(trayImage);
    trayIcon -> setContextMenu(trayIconMenu);

    //Подключаем обработчик клика по иконке
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    //Выводим значок
    trayIcon -> show();
}

void MainWindow::trayActionExecute()
{
    //QMessageBox::information(this, "TrayIcon", "Тестовое сообщение.");
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        //this -> trayActionExecute();
        break;
    default:
        break;
    }
}

void MainWindow::setTrayIconActions()
{
    //Настройки событий
    minimizeAction = new QAction("Свернуть", this);
    restoreAction = new QAction("Восстановить", this);
    quitAction = new QAction("Выход", this);

    //Подключение событий к слотам
    connect (minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));
    connect (restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
    connect (quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    //Добавление меню событий в трэй меню
    trayIconMenu = new QMenu(this);
    trayIconMenu -> addAction (minimizeAction);
    trayIconMenu -> addAction (restoreAction);
    trayIconMenu -> addAction (quitAction);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event -> type() == QEvent::WindowStateChange)
    {
        if (isMinimized())
        {
            this -> hide();
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}


