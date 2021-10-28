<?php
require_once __DIR__.'/vendor/autoload.php';
use Workerman\Lib\Timer;
use Workerman\Worker;
require_once 'dbwork.php';

//массив "пользователей"
$connections = [];
$worker = new Worker("websocket://127.0.0.1:61523");


$worker->onConnect = function($connection) use(&$connections)
{
    $connection->onWebSocketConnect = function($connection) use (&$connections)
    {
        // Достаём имя пользователя, если оно было указано
        if (isset($_GET['userName'])) {
            $originalUserName = preg_replace('/[^a-zA-Zа-яА-ЯёЁ0-9\-\_ ]/u', '', trim($_GET['userName']));
        }
        else {
            $originalUserName = 'Incognito';
        }        
        $userName = $originalUserName;
        
        $connection->userName = $userName;
        $connection->userOnline = 1;
        $connections[$connection->userName] = $connection;
        
        //массив пользователей для клинета
        $users = [];
        foreach ($connections as $c) {
            $users[] = [
                'userName' => $c->userName,
                'userOnline' => $c->userOnline
            ];
        }
        
        // Отправляем пользователю данные авторизации
        $messageData = [
            'action' => 'Authorized',
            'userName' => $connection->userName,
            'users' => $users
        ];
        $connection->send(json_encode($messageData));
        
        //запрос в БД для выдачи сообщений пока был в оффлайне
        $request = new DataBaseWork();
        $arr = $request->sendAllMessage($userName);
        foreach($arr as $value){
            $messageData = [
                'action' => 'PrivateMessage',
                'userName' => $value['sender'],
                'toUserName'=> $userName,
                'text' => $value['textOfMessage']
            ];
            $connection->send(json_encode($messageData));
        }

        //Затем удаляем все сообщения из БД и сам объект
        $request->deleteAllMessage($userName);
        unset($request);

        // Оповещаем всех пользователей о новом участнике в чате
        $messageData = [
            'action' => 'Connected',
            'userName' => $connection->userName
        ];
        $message = json_encode($messageData);
        foreach ($connections as $c) {
            $c->send($message);
        }
    };
};

$worker->onClose = function($connection) use(&$connections)
{
    $connection->userOnline = 0;
    // Оповещаем всех пользователей о выходе участника из чата
    $messageData = [
        'action' => 'Disconnected',
        'userName' => $connection->userName
    ];
    $message = json_encode($messageData);
    foreach ($connections as $c) {
        $c->send($message);
    }
};

$worker->onWorkerStart = function($worker) use (&$connections)
{
};
$worker->onMessage = function($connection, $message) use (&$connections)
{
    $messageData = json_decode($message, true);
    //получатель
    $toUserName = isset($messageData['toUserName']) ? (string) $messageData['toUserName'] : "";
    //тип сообщения
    $action = isset($messageData['action']) ? $messageData['action'] : '';
    //в сети ли получатель
    $toUserOnline = isset($messageData['toUserOnline']) ? (int) $messageData['toUserOnline'] : 0;
    //отправитель
    $messageData['userName'] = $connection->userName;
    $messageData['text'] = htmlspecialchars($messageData['text']);
    $messageData['text'] = preg_replace('/\{(.*)\}/u', '<b>\\1</b>', $messageData['text']);
      
    if ($toUserName == "") {
        // Отправляем сообщение всем пользователям
        $messageData['action'] = 'PublicMessage';
        foreach ($connections as $c) {
            $c->send(json_encode($messageData));
        }
    }
    else {
        //Отправляем личное сообщение
        $messageData['action'] = 'PrivateMessage';  
        if (isset($connections[$toUserName])) {
            if ($toUserOnline == 0){
                //если получатель не в сети
                $request = new DataBaseWork();
                $request->addMessage($messageData['userName'],$messageData['toUserName'],$messageData['text']);
                unset($request);
            }
            else {
                //если получатель в сети    
                $connections[$toUserName]->send(json_encode($messageData));
                $connection->send(json_encode($messageData)); 
            }
        }
        else {
            //в случае непридвиденного сообщения
            $messageData['text'] = 'Не удалось отправить сообщение выбранному пользователю';
            $connection->send(json_encode($messageData));
        }
    }
};
Worker::runAll();