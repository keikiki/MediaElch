#ifndef CONCERTMODEL_H
#define CONCERTMODEL_H

#include <QAbstractItemModel>
#include <QIcon>

#include "data/Concert.h"

/**
 * @brief The ConcertModel class
 */
class ConcertModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ConcertRoles
    {
        NameRole = Qt::UserRole + 1,
        FileNameRole
    };
    explicit ConcertModel(QObject *parent = nullptr);
    void addConcert(Concert *concert);
    void clear();
    QList<Concert *> concerts();
    Concert *concert(int row);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int countNewConcerts() const;
    void update();

private slots:
    void onConcertChanged(Concert *concert);

private:
    QList<Concert *> m_concerts;
    QIcon m_newIcon;
    QIcon m_syncIcon;
};

#endif // CONCERTMODEL_H
