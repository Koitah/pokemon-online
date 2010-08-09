#include <QtCore>
#include <ctime>
#include "../PokemonInfo/pokemoninfo.h"

/*
 * First we move to a tier.
 * we get the usage of each pokemon. (global)
 * we get the total usage for the tier
 * we create the page of each pokemon, in xml.
 * we create the pages for each pokemon, in html.
 * we create the pages of the tiers in xml.
 * we create the pages of the tiers in html
 * we create the index in html
 */

QString directory;

static void recurseRemove(const QString &path) {
    QDir d(path);

    QStringList files = d.entryList(QDir::Files | QDir::Hidden | QDir::System);

    foreach(QString file, files) {
        d.remove(file);
    }

    QStringList dirs = d.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);

    foreach(QString dir, dirs) {
        recurseRemove(d.absoluteFilePath(dir));
    }

    d.rmdir(d.absolutePath());
}

class Skeleton {
public:
    Skeleton(const QString &path);

    void addDefaultValue(const char *member, const QVariant &value);
    Skeleton &appendChild(const QString &item);

    QString generate() const;
private:
    Skeleton *top;
    Skeleton(Skeleton *top, const QString &body);
    Skeleton *parent () const {
        return top;
    }
    bool isBoss() const {
        return top == NULL;
    }

    /* {moveset *} */
    QHash<QString, QList<Skeleton> > children;
    /* {pokemon} */
    QHash<QString, QString> values;
    /* moveset{{
       xxx
       }} */
    QHash<QString, QString> bodies;
    /* page {{
       }} */
    QString body;

    QString getSkeletonBody(const QString &name) const;
};

Skeleton::Skeleton(const QString &path) {
    QFile f(path);
    f.open(QIODevice::ReadOnly);

    QString text = QString(f.readAll());

    QRegExp r("([a-z]+)\\{\\{((?:[^}].?)*)\\}\\}", Qt::CaseSensitive, QRegExp::Wildcard);
    r.setPatternSyntax(QRegExp::RegExp2);
    int pos = 0;

    while (r.indexIn(text, pos) > -1) {
        pos += r.matchedLength();

        if (r.cap(1) == "page") {
            body = r.cap(2);
        } else {
            bodies[r.cap(1)] = r.cap(2);
        }
    }

    top = NULL;
}

Skeleton::Skeleton(Skeleton *top, const QString &body) : top(top), body(body) {
}

void Skeleton::addDefaultValue(const char *member, const QVariant &value) {
    values[member] = value.toString();
}

Skeleton & Skeleton::appendChild(const QString &item) {
    children[item].push_back(Skeleton(parent() == NULL ? this : parent(), getSkeletonBody(item)));
    return children[item].back();
}

QString Skeleton::getSkeletonBody(const QString &name) const {
    return isBoss() ? bodies.value(name) : parent()->getSkeletonBody(name);
}

QString Skeleton::generate() const {
    QHashIterator<QString, QList<Skeleton> > it(children);

    QString body = this->body;

    while (it.hasNext()) {
        it.next();

        if (it.value().size() == 0) {
            continue;
        }

        if (body.indexOf(QString("{%1}").arg(it.key())) != -1) {
            body.replace(QString("{%1}").arg(it.key()), it.value().front().generate());
        } else if (body.indexOf(QString("{%1*}").arg(it.key())) != -1) {
            QString remplacement;

            for (int i = 0; i < it.value().size(); i++) {
                remplacement += it.value()[i].generate();
            }

            body.replace(QString("{%1*}").arg(it.key()), remplacement);
        }
    }

    QHashIterator <QString, QString> it2(values);
    while (it2.hasNext()) {
        it2.next();
        body.replace(QString("{%1}").arg(it2.key()), it2.value());
    }

    body.replace(QRegExp("\\{[^}]*\\}"), "");

    return body;
}

struct SecondaryStuff {
    quint8 evs[6];
    quint8 dvs[6];
    quint8 nature;
    int usage;

    bool operator < (const SecondaryStuff &other) const {
        for(int i = 0 ; i < 6; i++) {
            if (evs[i] != other.evs[i])
                return (evs[i] < other.evs[i]);
            if (dvs[i] != other.dvs[i])
                return (dvs[i] < other.dvs[i]);
        }
        return (nature < other.nature);
    }

    bool operator == (const SecondaryStuff &other) const {
        return memcmp(evs, other.evs, sizeof(evs)) == 0 &&
                memcmp(dvs, other.dvs, sizeof(dvs)) == 0 &&
                nature == other.nature;
    }

    SecondaryStuff & operator += (const SecondaryStuff &other) {
        usage += other.usage;

        return *this;
    }

    void complete(Skeleton &s, bool hiddenPower) const;
};

void SecondaryStuff::complete(Skeleton &s, bool hiddenPower) const
{
    static const char* stats[] = {"HP", "Atk", "Def", "Spd", "SAtk", "SDef"};

    s.addDefaultValue("nature", NatureInfo::Name(nature));

    QStringList inc;

    for (int i = 0; i < 6; i++) {
        if (evs[i] != 0)
            inc.append(QString("%1 %2").arg(evs[i]).arg(stats[i]));
    }

    if (inc.size() > 0) {
        s.addDefaultValue("evs", inc.join(" / "));
    } else {
        s.addDefaultValue("evs", 0);
    }

    inc.clear();

    for (int i = 0; i < 6; i++) {
        if (dvs[i] != 31) {
            inc.append(QString("%1 %2").arg(dvs[i]).arg(stats[i]));
        }
    }

    if (inc.size() > 0) {
        s.addDefaultValue("ivs", inc.join(" / "));
    } else {
        s.addDefaultValue("ivs", "All 31");
    }

    if (hiddenPower) {
        int type = HiddenPowerInfo::Type(dvs[0], dvs[1], dvs[2], dvs[3], dvs[4], dvs[5]);
        QString name = TypeInfo::Name(type);

        s.addDefaultValue("hiddenpower", QString("[<span class='%1'>%2</span>]").arg(name.toLower(), name));
    }
}

struct RawSet {
    quint16 level;
    quint16 item;
    quint16 moves[4];

    bool operator < (const RawSet &other) const {
        if (item != other.item) {
            return item < other.item;
        }

        if (level != other.level) {
            return level < other.level;
        }

        for (int i = 0; i < 4; i++) {
            if (moves[i] != other.moves[i]) {
                return moves[i] < other.moves[i];
            }
        }
        return 0;
    }
};

struct MoveSet {
    quint16 abilities[2];
    quint16 usage;
    quint16 num;
    RawSet raw;

    QMap<SecondaryStuff, SecondaryStuff> options;

    MoveSet();
    MoveSet(char buffer[28], int usage, int defAb);

    MoveSet & operator += (const MoveSet &other) {
        usage += other.usage;

        abilities[0] += other.abilities[0];
        abilities[1] += other.abilities[1];

        foreach(const SecondaryStuff &option, other.options) {
            if (options.contains(option)) {
                options[option] += option;
            } else {
                options.insert(option, option);
            }
        }

        return *this;
    }

    void complete(Skeleton &m) const;

    bool hasHiddenPower() const;
};

MoveSet::MoveSet()
{
}

MoveSet::MoveSet(char buffer[28], int usage, int defAb)
    : usage(usage)
{
    qint32 *buf = (qint32 *) buffer;

    raw.item = buf[0] & 0xFFFF;
    num = buf[0] >> 16;

    if (defAb == buf[1] >> 16) {
        abilities[0] = usage;
        abilities[1] = 0;
    } else {
        abilities[0] = 0;
        abilities[1] = usage;
    }

    raw.level = buf [1] & 0xFF;

    SecondaryStuff s;

    s.usage = usage;
    s.nature = buf[2] >> 24;
    s.evs[0] = (buf[2] >> 16) & 0xFF;
    s.evs[1] = (buf[2] >> 8) & 0xFF;
    s.evs[2] = buf[2] & 0xFF;
    s.evs[3] = buf[3] >> 16;
    s.evs[4] = (buf[3] >> 8) & 0xFF;
    s.evs[5] = buf[3] & 0xFF;

    for (int i = 0; i < 6; i++) {
        s.dvs[i] = (buf[4] >> (5-i)*5) & 0x1F;
    }

    qint16 *moves = (qint16 *) (&buf[5]);

    qSort(&moves[0], &moves[4]);

    for (int i =0; i < 4; i++) {
        raw.moves[i] = moves[i];
    }

    options.insert(s,s);
}

bool MoveSet::hasHiddenPower() const {
    return raw.moves[0] == Move::HiddenPower ||
            raw.moves[1] == Move::HiddenPower ||
            raw.moves[2] == Move::HiddenPower ||
            raw.moves[3] == Move::HiddenPower;
}

void MoveSet::complete(Skeleton &m) const
{
    QList<int> ab = PokemonInfo::Abilities(num);
    int tot = abilities[0] + abilities[1];

    m.addDefaultValue("pokemon", PokemonInfo::Name(num));
    m.addDefaultValue("item", ItemInfo::Name(raw.item));
    m.addDefaultValue("level", raw.level);
    m.addDefaultValue("abilities", abilities[0]*abilities[1] == 0 ? (abilities[0] == 0 ? AbilityInfo::Name(ab[1]) : AbilityInfo::Name(ab[0]))
        : QString("%1 (%2 %) / %3 (%4 %)").arg(AbilityInfo::Name(ab[0])).arg(double(100*abilities[0])/tot,0,'f',1)
        .arg(AbilityInfo::Name(ab[1])).arg(double(100*abilities[1])/tot,0,'f',1).toUtf8());


    QMultiMap<int, SecondaryStuff> usageMap;
    foreach(const SecondaryStuff &s, options)  {
        usageMap.insertMulti(s.usage, s);
    }

    QMapIterator<int, SecondaryStuff> it(usageMap);
    it.toBack();

    it.previous();

    Skeleton &s = m.appendChild("firststatset");
    s.addDefaultValue("percentage", QString::number(double(100*it.value().usage)/usage, 'f', 1));

    bool hp = hasHiddenPower();
    it.value().complete(s, hp);

    while (it.hasPrevious()) {
        it.previous();
        Skeleton &s = m.appendChild("statset");
        s.addDefaultValue("percentage", QString::number(double(100*it.value().usage)/usage, 'f', 1));
        it.value().complete(s, hp);
    }

    m.addDefaultValue("move1", MoveInfo::Name(raw.moves[0]));
    m.addDefaultValue("move2", MoveInfo::Name(raw.moves[1]));
    m.addDefaultValue("move3", MoveInfo::Name(raw.moves[2]));
    m.addDefaultValue("move4", MoveInfo::Name(raw.moves[3]));
}

static QString getImageLink(int pokemon)
{
    return QString("%1/DP%2.png").arg(pokemon).arg(PokemonInfo::Gender(pokemon) == Pokemon::FemaleAvail ? "f" : "m");
}

static QString getIconLink(int pokemon)
{
    return QString("%1.PNG").arg(pokemon);
}

struct Bcc {
    QByteArray buffer;
    int usage;
    int leadUsage;

    Bcc(const QByteArray &a, int usage, int leadUsage):
            buffer(a), usage(usage), leadUsage(leadUsage) {

    }
};

struct GlobalThings {
    QHash<int, int> moves;
    QHash<int, int> items;
    int abilities[2];

    int totalMoves;
    int totalItems;

    GlobalThings() {
        totalItems = 0;
        totalMoves = 0;
        abilities[0] = 0;
        abilities[1] = 1;
    }
};

void addMoveset(QMap<RawSet, MoveSet> &container, char *buffer, int usage, int defAb, GlobalThings &globals) {
    if (usage == 0)
        return;
    MoveSet m = MoveSet(buffer, usage, defAb);

    globals.abilities[0] += m.abilities[0];
    globals.abilities[1] += m.abilities[1];
    globals.items[m.raw.item] += m.usage;
    globals.totalItems += m.usage;

    for (int i = 0; i < 4; i++) {
        if (m.raw.moves[i] != 0) {
            globals.moves[m.raw.moves[i]] += m.usage;
        }
    }
    globals.totalMoves += m.usage;

    if (!container.contains(m.raw)) {
        container.insert(m.raw, m);
    } else {
        container[m.raw] += m;
    }
}

void parseMovesets(Skeleton &s, QMap<RawSet, MoveSet> &movesets, const QString &key, int totalUsage) {
    QMultiMap <int, MoveSet> usageOrder;

    QMapIterator<RawSet, MoveSet> mit (movesets);

    while (mit.hasNext()) {
        mit.next();

        usageOrder.insertMulti(mit.value().usage, mit.value());
    }

    QMapIterator<int, MoveSet> usageIt(usageOrder);

    usageIt.toBack();
    int i = 0;

    while (usageIt.hasPrevious() && i < 25) {
        usageIt.previous();
        i+= 1;

        Skeleton &m = s.appendChild(key);
        m.addDefaultValue("rank", i);
        m.addDefaultValue("percentage", QString::number(double(100*usageIt.key())/totalUsage,'f',2));
        m.addDefaultValue("battles", usageIt.key());
        usageIt.value().complete(m);
    }
}

void parseGlobals(Skeleton &s, QHash<int, int> &movesets, int totalUsage, const QString &bigkey, const char *smallkey, QString (*f)(int)) {
    QMultiMap <int, int> usageOrder;

    QHashIterator<int, int> mit (movesets);

    while (mit.hasNext()) {
        mit.next();

        usageOrder.insertMulti(mit.value(), mit.key());
    }

    QMapIterator<int, int> usageIt(usageOrder);

    usageIt.toBack();
    int i = 0;

    while (usageIt.hasPrevious() && i < 25) {
        usageIt.previous();
        i+= 1;

        Skeleton &m = s.appendChild(bigkey);
        m.addDefaultValue(smallkey, f(usageIt.value()));
        m.addDefaultValue("percentage", QString::number(double(100*usageIt.key())/totalUsage,'f',2));
        m.addDefaultValue("rank", i);
        m.addDefaultValue("battles", usageIt.key());
    }
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    PokemonInfoConfig::setConfig(PokemonInfoConfig::NoGui);
    PokemonInfo::init("db/pokes/");
    MoveInfo::init("db/moves/");
    AbilityInfo::init("db/abilities/");
    ItemInfo::init("db/items/");
    NatureInfo::init("db/natures/");
    HiddenPowerInfo::init("db/types/");
    TypeInfo::init("db/types/");

    srand(time(NULL));
    for (int i = 0; i < 100; i++)
        rand();

    QByteArray data;

    for (int i = 0; i < 4; i++)
        data.append(QByteArray::number(rand()));

    /* For the commands we're using, the '\\' for windows is necessary */
#ifdef WIN32
    QString dirname = "usage_stats\\.tmp" + QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(10);
#else
    QString dirname = "usage_stats/.tmp" + QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(10);
#endif

    /* First, copy the files to another directory */
    QDir d;
    d.mkpath(dirname);
    d.cd(dirname);

    fprintf(stdout, "Moving raw files to a safe directory... (may take time)");

#ifdef WIN32
    system( ("xcopy usage_stats\\raw\\* " + dirname + " /s > copy.txt").toAscii().data() );
#else
    system( ("cp -R usage_stats/raw " + dirname).toAscii().data() + " > copy.txt" );
#endif

    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    QList<QPair<QString, int> > mostUsedPokemon;

    foreach(QString dir, dirs) {
        d.cd(dir);

        QStringList files = d.entryList(QDir::Files);

        /* First we get the usage of each pokemon */
        QHash<int, int> usage;
        QHash<int, int> leadUsage;
        qint32 totalusage(0);

        QHash<int, QList<Bcc> > buffers;

        fprintf(stdout, "\nDoing Tier %s\n", dir.toUtf8().data());

        foreach(QString file, files) {
            FILE *f = fopen(d.absoluteFilePath(file).toAscii().data(), "rb");

            char buffer[28];

            while (fread(buffer, sizeof(char), 28/sizeof(char), f) == 28) {
                qint32 iusage(0), ileadusage(0);

                fread(&iusage, sizeof(qint32), 1, f);
                fread(&ileadusage, sizeof(qint32), 1, f);

                int pokenum = (*((qint32*) buffer)) >> 16;

                if (pokenum != 0) {
                    usage[pokenum] += iusage;
                    if (ileadusage > 0) {
                        leadUsage[pokenum] += ileadusage;
                    }
                    buffers[pokenum].append(Bcc(QByteArray(buffer, 28), iusage, ileadusage));
                }
                totalusage += iusage;
            }

            fclose(f);
        }

        QDir outDir;
        outDir.mkpath("usage_stats/formatted/" + dir);
        outDir.cd("usage_stats/formatted/" + dir);

        int totalBattles = totalusage/6;

        Skeleton tierSk("usage_stats/formatted/tier_page.template");
        tierSk.addDefaultValue("tier", dir);
        tierSk.addDefaultValue("battles", totalBattles/2);

        QHashIterator<int, int> hit(usage);
        QMultiMap<int, int> reverseUsage;

        while (hit.hasNext()) {
            hit.next();
            reverseUsage.insert(hit.value(), hit.key());
        }

        int i = 0;

        QMapIterator<int, int> it(reverseUsage);
        it.toBack();

        while (it.hasPrevious()) {
            i += 1;
            it.previous();
            Skeleton &childSk = tierSk.appendChild(i <= 5 ? "toppokemon" : "lowpokemon");
            childSk.addDefaultValue("rank", i);
            childSk.addDefaultValue("imagelink", getImageLink(it.value()));
            childSk.addDefaultValue("iconlink", getIconLink(it.value()));
            childSk.addDefaultValue("pokemonlink", QString("%1.html").arg(it.value()));
            childSk.addDefaultValue("percentage", QString::number(double(100*it.key())/totalBattles,'f',2));
            childSk.addDefaultValue("pokemon", PokemonInfo::Name(it.value()));
        }

        QFile index(outDir.absoluteFilePath("index.html"));
        index.open(QIODevice::WriteOnly);
        index.write(tierSk.generate().toUtf8());
        index.close();

        it.toBack();

        while (it.hasPrevious()) {
            it.previous();

            int pokemon = it.value();

            int normalUsage = it.key() - leadUsage[pokemon];

            fprintf(stdout, "Doing Pokemon %s\n", PokemonInfo::Name(pokemon).toUtf8().data());

            QMap<RawSet, MoveSet> movesets;
            QMap<RawSet, MoveSet> leadsets;
            GlobalThings globals;
            int defAb = PokemonInfo::Abilities(pokemon)[0];

            foreach(Bcc b, buffers[pokemon]) {
                char *buffer = b.buffer.data();

                addMoveset(movesets, buffer, b.usage-b.leadUsage, defAb, globals);
                addMoveset(leadsets, buffer, b.leadUsage, defAb, globals);
            }

            Skeleton s("usage_stats/formatted/pokemon_page.template");
            s.addDefaultValue("pokemon", PokemonInfo::Name(pokemon));
            s.addDefaultValue("tier", dir);
            s.addDefaultValue("imagelink", getImageLink(pokemon));
            s.addDefaultValue("percentage", QString::number(double(100*it.key())/totalBattles,'f',2));
            s.addDefaultValue("battles", it.key());
            s.addDefaultValue("nonleadpercentage", QString::number(double(100*normalUsage)/totalBattles,'f',2));
            s.addDefaultValue("nonleadbattles", normalUsage);
            s.addDefaultValue("leadpercentage", QString::number(double(100*leadUsage[pokemon])/totalBattles,'f',2));
            s.addDefaultValue("leadbattles", leadUsage[pokemon]);

            parseMovesets(s, movesets, "moveset", normalUsage);
            parseMovesets(s, leadsets, "leadmoveset", leadUsage[pokemon]);
            parseGlobals(s, globals.moves, globals.totalMoves, "globalmove", "move", &MoveInfo::Name);
            parseGlobals(s, globals.items, globals.totalItems, "globalitem", "item", &ItemInfo::Name);
            QHash<int, int> abilities;
            abilities[defAb] = globals.abilities[0];
            int totAbilities = globals.abilities[0];
            if (globals.abilities[1] > 0 && PokemonInfo::Abilities(pokemon)[1] != 0) {
                abilities[PokemonInfo::Abilities(pokemon)[1]] = globals.abilities[1];
                totAbilities += globals.abilities[1];
            }
            parseGlobals(s, abilities, totAbilities, "globalability", "ability", &AbilityInfo::Name);

            QFile pokef(outDir.absoluteFilePath("%1.html").arg(pokemon));
            pokef.open(QIODevice::WriteOnly);
            pokef.write(s.generate().toUtf8());
        }

        mostUsedPokemon.push_back(QPair<QString, int> (dir, reverseUsage.size() > 0 ? (--reverseUsage.end()).value() : 0));
        d.cdUp();
    }

    typedef QPair<QString, int> pair;

    Skeleton indexSk("usage_stats/formatted/index.template");
    foreach(pair p, mostUsedPokemon) {
        if (p.second == 0)
            continue;
        Skeleton &pok = indexSk.appendChild("tier");

        pok.addDefaultValue("tier", p.first);
        pok.addDefaultValue("pokemon", PokemonInfo::Name(p.second));
        pok.addDefaultValue("imagelink", getImageLink(p.second));
        pok.addDefaultValue("iconlink", getIconLink(p.second));
    }

    QFile f("usage_stats/formatted/index.html");
    f.open(QIODevice::WriteOnly);
    f.write(indexSk.generate().toUtf8());

    recurseRemove(dirname);

    return 0;
}
