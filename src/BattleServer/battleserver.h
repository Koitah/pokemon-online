#ifndef BATTLESERVER_H
#define BATTLESERVER_H

#include <QObject>
#include <QHash>

#include <Utilities/contextswitch.h>
#include <Utilities/asiosocket.h>

class BattleServerPluginManager;
class ServerConnection;
class BattlePlayer;
class ChallengeInfo;
class TeamBattle;

class BattleServer : public QObject
{
    Q_OBJECT

    friend class ConsoleReader;
public:
    explicit BattleServer(QObject *parent = 0);
    
    void start(int port, bool closeOnDc);
    void changeDbMod(const QString &mod);
signals:
    
public slots:
    void print(const QString &s);
    void newConnection();

    void newBattle(int sid, int battleid, const BattlePlayer &pb1, const BattlePlayer &pb2, const ChallengeInfo &c, const TeamBattle &t1, const TeamBattle &t2);
    void onError(int id);

    void modChanged(const QString &);
    void loadPlugin(const QString &path);
    void unloadPlugin(const QString &name);
private:
    int freeid() const;

#ifndef BOOST_SOCKETS
    QTcpServer * server;
#else
    GenericSocket server;
    SocketManager manager;
#endif

    BattleServerPluginManager *pluginManager;
    ContextSwitcher battleThread;
    mutable int servercounter;

    QHash<int, ServerConnection*> connections;
    bool closeOnDc;
};

#endif // BATTLESERVER_H
