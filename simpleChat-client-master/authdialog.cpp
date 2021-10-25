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

#include "authdialog.h"
#include "ui_authdialog.h"

AuthDialog::AuthDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AuthDialog)
{
    ui->setupUi(this);

    connect(ui->pushButton_reject, &QPushButton::clicked,
            this, &AuthDialog::reject);
    connect(ui->pushButton_connect, &QPushButton::clicked,
            this, &AuthDialog::accept);
}

AuthDialog::~AuthDialog()
{
    delete ui;
}

void AuthDialog::setConnectionData(const AuthDialog::ConnectionData &connectionData)
{
    ui->lineEdit_server->setText(connectionData.server);
    ui->lineEdit_port->setText(QString::number(connectionData.port));
    ui->lineEdit_userName->setText(connectionData.userName);
}

AuthDialog::ConnectionData AuthDialog::connectionData() const
{
    ConnectionData connectionData;
    connectionData.server = ui->lineEdit_server->text();
    connectionData.port = ui->lineEdit_port->text().toInt();
    connectionData.userName = ui->lineEdit_userName->text();

    return connectionData;
}
