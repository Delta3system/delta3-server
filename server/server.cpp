#include "server.h"

#include "client.h"
#include "utils.h"
#include "application.h"

namespace
{
    const quint16 DEFAULT_PORT = 1235;
    const quint16 MAX_INACTIVE_TIME = 15;
    const quint16 TIME_TO_PING = 5;
}

namespace delta3
{
    Server::Server(QObject *parent):
        QObject(parent),
        _tcpServer(new QTcpServer(this)),
        _storage(new ClientInfoStorage(this))
    {
        connect(_tcpServer,SIGNAL(newConnection()),
                this,SLOT(onNewConnection()));
        _storage->load();

        _logger.openLogFile(settings()->value("general/logfile", LOG_FILE).toString());
        _logger.setDefaultStream(Logger::FILE);
        _logger.message() << Logger::toChar(tr("Delta3 Server started"));
        _logger.write();
        qDebug() << tr("Delta3 Server started");
    }

    Server::~Server()
    {
    }

    bool Server::start()
    {
        startTimer( DEFAULT_TIMER_INTERVAL );
        return _tcpServer->listen(QHostAddress(
             settings()->value("network/listen_addr", "0.0.0.0").toString()),
             settings()->value("network/listen_port", DEFAULT_PORT).toInt());
    }

    void Server::onNewConnection()
    {
        Client *client=new Client(
                _tcpServer->nextPendingConnection(),
                _storage, this);
        _clients.insert(client->getId(),client);
    }

    QByteArray Server::listConnectedClients()
    {
        QByteArray result;
        qint16 clientNum=0;
        for (auto i=_clients.begin();i!=_clients.end();i++)
        {
            if (i.value()->getStatus()==ST_CLIENT)
            {
                QByteArray clientInfo;
                clientInfo.append( toBytes((qint16)i.key()) );
                clientInfo.append( i.value()->getIdHash() );
                clientInfo.append( toBytes(i.value()->getOs(), 20 ), 20 );
                clientInfo.append( toBytes(i.value()->getDevice(), 20 ), 20 );
                clientInfo.append( toBytes(i.value()->getIp()), 4);
                clientInfo.append( toBytes(i.value()->getCaption(), 30 ), 30 );
                result.append(clientInfo);
                clientNum++;
            }
        }
        result=toBytes(clientNum)+result;
        return result;
    }

    Clients::iterator Server::searchClient(qint32 clientId)
    {
        return _clients.find(clientId);
    }

    Clients::iterator Server::clientEnd()
    {
        return _clients.end();
    }

    void Server::timerEvent( QTimerEvent* event )
    {
        Q_UNUSED( event );

        for (auto i=_clients.begin();i!=_clients.end();i++)
        {
            if (i.value()->getStatus()==ST_DISCONNECTED)
            {
                delete i.value();
                i = _clients.erase(i);
                continue;
            }

            if (i.value()->getLastSeen()>MAX_INACTIVE_TIME)
            {
                _logger.message()
                        << tr("Disconnecting inactive client: ")
                        << QHostAddress(i.value()->getIp()).toString().toLocal8Bit().data();
                _logger.write();
                qDebug()<< tr("Disconnecting inactive client: ")
                        << QHostAddress(i.value()->getIp()).toString().toLocal8Bit().data();
                i.value()->disconnectFromHost();
                continue;
            }

            if (i.value()->getLastSeen()>TIME_TO_PING)
            {
                i.value()->ping();
            }
        }
        _storage->save();
    }

    void Server::resendListToAdmins()
    {
        QByteArray clientList=listConnectedClients();
        for (auto i=_clients.begin();i!=_clients.end();i++)
            if (i.value()->getStatus()==ST_ADMIN)
                i.value()->sendList(clientList);
    }

    void Server::setClientCaption(qint16 clientId, const QString& caption)
    {
        auto i=_clients.find(clientId);
        if (i==_clients.end())
            return;
        i.value()->setCaption(caption);
        _storage->setCaption(i.value()->getIdHash(),caption);
        i.value()->getIdHash();
    }

    void Server::setAdminTalkingWithClient(qint16 clientId, qint16 adminId)
    {
        auto i=_clients.find(clientId);
        if (i==_clients.end())
            return;
        i.value()->addTalkingWithAdmin(adminId);
    }

    QSettings* Server::settings()
    {
        return ((Application*)(parent()))->getSettings();
    }
}
