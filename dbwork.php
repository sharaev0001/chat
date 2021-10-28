<?php

interface IDataBaseWork
{
    public function __construct();
    public function addMessage($sender, $receiver, $textOfMessage);
    public function deleteAllMessage($receiver);
    public function sendAllMessage($receiver);
}


Class DataBaseWork implements IDataBaseWork
{
    private $pdo;
    public function __construct()
    {
        $this->pdo = new PDO('mysql:host=localhost;dbname=message', 'root', 'password');
    }
    public function addMessage($sender, $receiver, $textOfMessage)
    {
        return $this->pdo->exec("INSERT INTO unreadmessage VALUES (0, '$sender', '$receiver', '$textOfMessage')");
    }
    public function deleteAllMessage($receiver)
    {
        return $this->pdo->exec("DELETE FROM unreadmessage WHERE receiver = '$receiver'");
    }
    public function sendAllMessage($receiver)
    {
        $array = $this->pdo->query("SELECT * FROM unreadmessage WHERE receiver LIKE '$receiver'");
        $resArr = array();
        while($catalog = $array->fetch()){
            $resArr[] = ['sender'=>$catalog['sender'],'textOfMessage'=>$catalog['textofmessage']];
        }
        return $resArr;
    }

}