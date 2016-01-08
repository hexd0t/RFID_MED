#ifndef DBPROP_H
#define DBPROP_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <assert.h>
#include <QString>
#include <QVariant>

#define DBP_TEMPPARA <T, table, valcol, idcol>
#define DBP_TEMPLATE template <typename T, typename table, typename valcol, typename idcol>

// ================================== Beginn schwarze Compilermagie ======================================
//compile-time string-concat, grob nach
// https://stackoverflow.com/questions/9050857/constexpr-with-string-operations-workaround/9054709#9054709
template <char ... characters>
struct templateString
{
    static QString string()
    {
        constexpr const char stringcont[] {characters...};
        return QString::fromLatin1(stringcont, sizeof(stringcont));
    }
};

template <class L, class R>
struct templateConcat {};

template <char ... LC, char  ... RC>
struct templateConcat<templateString<LC ... >, templateString<RC ... >>
{
    typedef templateString<LC ..., RC ... > Result;
};

//String literal zu variadic char template conv
template<int len, const char str[len], int c>
struct constStrConv {//Zeichen anhängen, vorderen Teil rekursiv erzeugen
    typedef typename constStrConv<len, str, c-1>::Result prevIteration;
    typedef typename templateConcat<prevIteration, templateString<str[c]>>::Result Result;
};
template<int len, const char str[len]>
struct constStrConv<len, str, 0> { //Ende der Iteration, erstes Zeichen zurückggeben
    typedef templateString<str[0]> Result;
};
template<int len, const char str[len]>
struct constString {
    typedef typename constStrConv<len, str, len-2>::Result templateString;
    //-2, da letzter Index = sizeof -1 & Null abschneiden
};

#define constStr(varname, content) static constexpr const char DBP_##varname [] = content; \
typedef typename constString<sizeof(content), DBP_##varname>::templateString varname;

// ================================== Ende schwarze Compilermagie ======================================
//Superklasse, die nur vom Typ abhängt, um Arrays von dbprops erzeugen zu können
template<typename T>
class dbpropg {
public:
    dbpropg();
    virtual ~dbpropg();
    virtual T Get() = 0;
    virtual void Set(T val) =0;
};

DBP_TEMPLATE
class dbprop : public dbpropg<T>
{
public:
    dbprop();
    dbprop(const dbprop DBP_TEMPPARA & source);
    virtual ~dbprop();
    void init(QSqlDatabase* database, uint pid);
    virtual T Get();
    virtual void Set(T val);

private:
    uint parentId;
    bool initialized;
    QSqlQuery sqlUpdate;
    QSqlQuery sqlSelect;

    //UPDATE table SET valcol = :val WHERE idcol = :id;
    constStr(SQL_UPDATE1, "UPDATE ")
    constStr(SQL_UPDATE2, " SET ")
    constStr(SQL_UPDATE3, "= :val WHERE ")
    constStr(SQL_UPDATE4, "= :id;")
    typedef typename templateConcat< SQL_UPDATE1,
        typename templateConcat<table,
        typename templateConcat<SQL_UPDATE2,
        typename templateConcat<valcol,
        typename templateConcat<SQL_UPDATE3,
        typename templateConcat<idcol, SQL_UPDATE4>
        ::Result>::Result>::Result>::Result>::Result>::Result SQL_UPDATE;

    //SELECT valcol FROM table WHERE idcol = :id LIMIT 1;
    constStr(SQL_SELECT1, "SELECT ")
    constStr(SQL_SELECT2, " FROM ")
    constStr(SQL_SELECT3, " WHERE ")
    constStr(SQL_SELECT4, " = :id LIMIT 1;")
    typedef typename templateConcat< SQL_SELECT1,
        typename templateConcat<valcol,
        typename templateConcat<SQL_SELECT2,
        typename templateConcat<table,
        typename templateConcat<SQL_SELECT3,
        typename templateConcat<idcol, SQL_SELECT4>
        ::Result>::Result>::Result>::Result>::Result>::Result SQL_SELECT;
};

/******************************************************************************************************/

template<typename T>
dbpropg<T>::dbpropg()
{

}

template<typename T>
dbpropg<T>::~dbpropg()
{

}

DBP_TEMPLATE
dbprop DBP_TEMPPARA::dbprop() : dbpropg<T>(), initialized(false)
{
}

DBP_TEMPLATE
dbprop DBP_TEMPPARA::dbprop(const dbprop DBP_TEMPPARA &source)
    : parentId(source.parentId),  initialized(source.initialized),
      sqlUpdate(source.sqlUpdate), sqlSelect(source.sqlSelect)
{
}

DBP_TEMPLATE
dbprop DBP_TEMPPARA::~dbprop()
{
}

DBP_TEMPLATE
void dbprop DBP_TEMPPARA::init(QSqlDatabase *database, uint pid)
{
    parentId = pid;

    sqlUpdate = QSqlQuery(*database);
    sqlUpdate.setForwardOnly(true);
    sqlUpdate.prepare( SQL_UPDATE::string() );
    sqlUpdate.bindValue(":id", parentId);

    sqlSelect = QSqlQuery(*database);
    sqlSelect.setForwardOnly(true);
    sqlSelect.prepare( SQL_SELECT::string() );
    sqlSelect.bindValue(":id", parentId);

    initialized = true;
}

DBP_TEMPLATE
T dbprop DBP_TEMPPARA::Get()
{
    assert(initialized);

    if(!sqlSelect.exec() || !sqlSelect.next())
        throw std::runtime_error("DB-Fehler (lesen)");
    T value = qvariant_cast<T>(sqlSelect.value(0));
    sqlSelect.finish();
    return value;
}

DBP_TEMPLATE
void dbprop DBP_TEMPPARA::Set(T val)
{
    assert(initialized);

    sqlUpdate.bindValue(":val", val);
    if(!sqlUpdate.exec())
        throw std::runtime_error("DB-Fehler (schreiben)");
    sqlUpdate.finish();
}


#endif // DBPROP_H
