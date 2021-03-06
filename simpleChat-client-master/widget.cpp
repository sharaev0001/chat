/*******************************************************************************
 * MIT License
 *
 * This file is part of the SimpleChat project:
 * https://github.com/wxmaper/SimpleChat-client
 *
 * Copyright (c) 2019 Aleksandr Kazantsev (https://wxmaper.ru)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "widget.h"
#include "ui_widget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QSettings>
#include <QUrlQuery>
#include <QJsonArray>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    m_webSocket(new QWebSocket(QString("SimpleChatClient"),
                               QWebSocketProtocol::Version13,
                               this)),
    m_pingTimer(new QTimer(this))
{
    ui->setupUi(this);
    ui->textBrowser->setContextMenuPolicy(Qt::NoContextMenu);
    //по умолчанию отправляем в общий чат
    closePrivateMessage();

    //Закрываем личный чат при нажатии на крестик
    connect(ui->toolButton_closePrivateMessage, &QToolButton::clicked, this, &Widget::closePrivateMessage);

    //Включаем приватный чат при двойном клике на пользователя в списке
    connect(ui->listWidget_users, &QListWidget::itemDoubleClicked, this, &Widget::privateWithUserFromItem);

    // Блокируем поле ввода сообщения до тех пор, пока соединение с сервером не будет установлено
    ui->lineEdit_message->setEnabled(false);

    // Соединяем сигнал нажатия кнопки Return со слотом отправки сообщения
    connect(ui->lineEdit_message, &QLineEdit::returnPressed, this, &Widget::onReturnPressed);

    // Обработка клика по ссылкам
    connect(ui->textBrowser, &QTextBrowser::anchorClicked, this, &Widget::onAnchorClicked);

    // Таймер для пингования сервера, чтобы указать, что соединение все еще живо
    //connect(m_pingTimer, SIGNAL(timeout()), m_webSocket, SLOT(ping()));

    // Соединяем сигналы websocket-клиента Подключение к серверу
    connect(m_webSocket, &QWebSocket::connected, this, &Widget::onConnected);

    // Отключение от сервера
    connect(m_webSocket, &QWebSocket::disconnected, this, &Widget::onDisconnected);

    // Ошибки сокета
    connect(m_webSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));

    // Получение сообщения с сервера
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &Widget::onTextMessageReceived);

    restoreConnectionData();
}

Widget::~Widget()
{
    m_webSocket->close();
    delete ui;
}

void Widget::saveConnectionData()
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.setValue("server", m_connectionData.server);
    settings.setValue("port", m_connectionData.port);
    settings.setValue("userName", m_connectionData.userName);
}

void Widget::restoreConnectionData()
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    m_connectionData.server = settings.value("server", "127.0.0.1").toString();
    m_connectionData.port = settings.value("port", 61523).toInt();
    m_connectionData.userName = settings.value("userName", "Incognito").toString();
}

void Widget::connectToServer()
{
    AuthDialog authDialog(this);
    authDialog.setConnectionData(m_connectionData);
    int result = authDialog.exec();
    if (result == AuthDialog::Accepted) {
        m_connectionData = authDialog.connectionData();
        QString html = QString("%1 <span style='color:#7f8c8d'><i>Установка соединения с <b>%2:%3</b>...</span>")
                .arg(datetime())
                .arg(m_connectionData.server)
                .arg(m_connectionData.port);
        ui->textBrowser->append(html);
        m_webSocket->open(QUrl(QString("ws://%1:%2?userName=%3")
                               .arg(m_connectionData.server)
                               .arg(m_connectionData.port)
                               .arg(m_connectionData.userName)));
    }
    else {
        close();
        qApp->quit();
    }
}

void Widget::closePrivateMessage()
{
    // "" указывает на то, что отправляем сообщение в общий чат
    m_toUserName = "";
    ui->toolButton_closePrivateMessage->hide();
    ui->label_receiver->setText(QString("Отправить в общий чат"));
}

void Widget::privateWithUserFromItem(QListWidgetItem *item)
{
    m_toUserName = item->text();
    m_toUserOnline = item->data(Qt::UserRole).toInt();
    ui->toolButton_closePrivateMessage->show();
    ui->label_receiver->setText(QString("Отправить пользователю %1")
                                .arg(m_toUserName));
}

QString Widget::datetime()
{
    QString html = QString("<span style='color:#34495e'><b>[%1]</b></span>")
            .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss"));
    return html;
}

void Widget::onUserAuthorized(const QString &userName)
{
    // При авторизации сохраняем данные "о себе"
    m_userName = userName;
    QString html = QString("%1 <span style='color:#7f8c8d'><i>Вы авторизованы с именем <b>%2</b></span>")
            .arg(datetime())
            .arg(userName);
    ui->textBrowser->append(html);
}

void Widget::onUserConnected(const QString &userName)
{
    bool duplicate = false;
    for (int i = 0; i < ui->listWidget_users->count(); i++) {
        QListWidgetItem *item = ui->listWidget_users->item(i);
        if (item->text() == userName) {
            item->setData(Qt::UserRole,1);
            item->setIcon(QIcon(QString(":/icons/online-1.png")));
            duplicate = true;
            break;
        }
    }
    if(!duplicate)
        addUser(userName,1);

    QString html = QString("%1 <span style='color:#7f8c8d'><i><b><a href='action://putUserName?userName=%2'>%2</a></b> вошел в чат</i></span>")
            .arg(datetime())
            .arg(userName);
    ui->textBrowser->append(html);
}

void Widget::addUser(const QString &userName, int userOnline)
{
    QListWidgetItem *item = new QListWidgetItem;
    item->setText(userName);
    item->setIcon(QIcon(QString(":/icons/online-%1.png").arg(userOnline)));
    item->setData(Qt::UserRole,userOnline);
    ui->listWidget_users->addItem(item);
}

void Widget::addUsers(const QJsonArray &users)
{
    foreach (QJsonValue v, users) {
        QJsonObject user = v.toObject();
        QString userName = user.value("userName").toString();
        int userOnline = user.value("userOnline").toInt();
        if (userName == m_userName) {
            continue;
        }
        addUser(userName, userOnline);
    }
}

void Widget::onUserDisconnected(const QString &userName)
{
    removeUser(userName);
    QString html = QString("%1 <span style='color:#7f8c8d'><i><b><a href='action://putUserName?userName=%2'>%2</a></b> вышел из чата</i></span>")
            .arg(datetime())
            .arg(userName);
    ui->textBrowser->append(html);
}

void Widget::removeUser(const QString &userName)
{
    for (int i = 0; i < ui->listWidget_users->count(); i++) {
        QListWidgetItem *item = ui->listWidget_users->item(i);
        if (item->text() == userName) {
            item->setData(Qt::UserRole,0);
            item->setIcon(QIcon(QString(":/icons/online-0.png")));
            break;
        }
    }
}

void Widget::onPublicMessage(const QString &userName, const QString &text)
{
    if (text.contains("<b>" + m_userName + "</b>")) {
        qApp->beep();
        qApp->alert(this);
    }
    QString html = QString("%1 <b><a href='action://putUserName?userName=%2'>%2:</a></b> <span style='color:#34495e'>%3</span>")
            .arg(datetime())
            .arg(userName)
            .arg(text);
    ui->textBrowser->append(html);
}

void Widget::onPrivateMessage(const QString &userName,const QString &toUserName ,const QString &text)
{
    qApp->beep();
    qApp->alert(this);

    QString html = QString("%1 <b>%4</b> <b><a href='action://putUserName?userName=%2'>%2:</a></b> <span style='color:#34495e'>%3</span>")
            .arg(datetime())
            .arg(userName == m_userName ? toUserName : userName)
            .arg(text)
            .arg(userName == m_userName ? "Otpravil usery " : "Prinyal ot ");
    ui->textBrowser->append(html);
}

void Widget::onConnected()
{
    m_pingTimer->start(15 * 1000); // пингуем сервер каждые 15 сек
    QString html = QString("%1 <span style='color:#16a085'><i>Соединение установлено!</i></span>")
            .arg(datetime());
    ui->textBrowser->append(html);
    ui->lineEdit_message->setEnabled(true);
    saveConnectionData();
}

void Widget::onDisconnected()
{
    m_pingTimer->stop();
    ui->listWidget_users->clear();
    QString html = QString("%1 <span style='color:#c0392b'><i>Соединение разорвано.</i></span>")
            .arg(datetime());
    ui->textBrowser->append(html);
    ui->lineEdit_message->setEnabled(false);
    // Через пять сек мы снова пытаемся соединиться с сервером
    QTimer::singleShot(5000, this, &Widget::connectToServer);
}

void Widget::onError(QAbstractSocket::SocketError error)
{
    m_pingTimer->stop();
    ui->listWidget_users->clear();
    QString html = QString("%1 <span style='color:#c0392b'>Ошибка сокета №%2: %3</span>")
            .arg(datetime())
            .arg(error)
            .arg(m_webSocket->errorString());
    ui->textBrowser->append(html);
    ui->lineEdit_message->setEnabled(false);
}

void Widget::onReturnPressed()
{
    // Достаём сообщение из поля ввода, удалив лишние пробелы
    QString text = ui->lineEdit_message->text().trimmed();

    // Очищаем поле ввода сообщения
    ui->lineEdit_message->clear();

    // Пустые сообщения не отправляем
    if (text.isEmpty()) {
        return;
    }

    // Собираем данные сообщения
    QJsonObject messageData;
    messageData.insert("toUserName", m_toUserName);
    messageData.insert("text", text);
    messageData.insert("toUserOnline",m_toUserOnline);

    // Преобразуем JSON-объект в строку
    QByteArray message = QJsonDocument(messageData).toJson(QJsonDocument::Compact);

    // Отправляем данные
    m_webSocket->sendTextMessage(message);
}

void Widget::onAnchorClicked(const QUrl &url)
{
    // При клике на имя пользователя, вставляем его в поле ввода сообщения
    QUrlQuery query(url);

    QString text = ui->lineEdit_message->text();
    if (text.isEmpty()) {
        text = "{" + query.queryItemValue("userName") + "}, ";
    }
    else if (text.endsWith(' ')) {
        text += "{" + query.queryItemValue("userName") + "} ";
    }
    else {
        text += " {" + query.queryItemValue("userName") + "} ";
    }

    ui->lineEdit_message->setText(text);
    ui->lineEdit_message->setFocus();
}

void Widget::onTextMessageReceived(const QString &message)
{
    // Преобразуем полученное сообщение в JSON-объект
    QJsonObject messageData = QJsonDocument::fromJson(message.toUtf8()).object();
    QString action = messageData.value("action").toString();
    QString userName = messageData.value("userName").toString();

    if (action == "Authorized") {
        onUserAuthorized(userName);
        QJsonArray users = messageData.value("users").toArray();
        addUsers(users);
     }
     else if (action == "Connected") {
        onUserConnected(userName);
     }
     else if (action == "Disconnected") {
        onUserDisconnected(userName);
     }
     else if (action == "PublicMessage") {
        QString text = messageData.value("text").toString();
        onPublicMessage(userName, text);
     }
     else if (action == "PrivateMessage") {
        QString text = messageData.value("text").toString();
        QString toUserName = messageData.value("toUserName").toString();
        onPrivateMessage(userName,toUserName, text);
     }
     else {
        // неизвестное действие
        qWarning() << "unknown action: " << action;
     }
}
