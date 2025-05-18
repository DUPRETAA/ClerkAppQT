#include "mainwindow.h"
#include <QApplication>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QMessageBox>
#include <QDebug>
#include <QClipboard>

QClipboard *clipboard;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QSystemSemaphore semaphore("<uniq id>", 1);  // создаём семафор
    semaphore.acquire(); // Поднимаем семафор, запрещая другим экземплярам работать с разделяемой памятью

#ifndef Q_OS_WIN32
#endif

    QSharedMemory sharedMemory("<uniq id 2>");  // Создаём экземпляр разделяемой памяти
    bool is_running;            // переменную для проверки ууже запущенного приложения
    if (sharedMemory.attach())
    {                           // пытаемся присоединить экземпляр разделяемой памяти
        // к уже существующему сегменту
        is_running = true;      // Если успешно, то определяем, что уже есть запущенный экземпляр
    }
    else
    {
        sharedMemory.create(1); // В противном случае выделяем 1 байт памяти
        is_running = false;     // И определяем, что других экземпляров не запущено
    }
    semaphore.release();        // Опускаем семафор

    // Если уже запущен один экземпляр приложения, то сообщаем об этом пользователю
    // и завершаем работу текущего экземпляра приложения
    if (is_running)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Приложение уже запущено");
        msgBox.exec();
        return 1;
    }
    clipboard = QApplication::clipboard();
    MainWindow w;


    //w.show();    
    return a.exec();
}
