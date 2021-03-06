#include "BaseTreeModel.h"

BaseTreeItem::BaseTreeItem(BaseTreeItem* parent) : parent(parent) {
	if (parent != nullptr) {
		parent->appendChild(this);
	}
}

BaseTreeItem::~BaseTreeItem() {
	qDeleteAll(children);
}

void BaseTreeItem::appendChild(BaseTreeItem* item) {
	children.append(item);
}

void BaseTreeItem::removeChild(BaseTreeItem* item) {
	children.removeOne(item);
	delete item;
}

QVariant BaseTreeItem::data() const {
	return "dab";
}

int BaseTreeItem::row() const {
	if (parent)
		return parent->children.indexOf(const_cast<BaseTreeItem*>(this));

	return 0;
}

BaseTreeModel::BaseTreeModel(QObject* parent) : QAbstractProxyModel(parent) {
	rootItem = new BaseTreeItem();

	QFileIconProvider icons;
	folderIcon = icons.icon(QFileIconProvider::Folder);
}

BaseTreeModel::~BaseTreeModel() {
	delete rootItem;
}

int BaseTreeModel::rowCount(const QModelIndex& parent) const {
	if (parent.column() > 0) {
		return 0;
	}

	BaseTreeItem* parentItem;

	if (!parent.isValid()) {
		parentItem = rootItem;
	} else {
		parentItem = static_cast<BaseTreeItem*>(parent.internalPointer());
	}

	return parentItem->children.count();
}

int BaseTreeModel::columnCount(const QModelIndex& parent) const {
	return 1;
}

Qt::ItemFlags BaseTreeModel::flags(const QModelIndex& index) const {
	if (!index.isValid()) {
		return Qt::NoItemFlags;
	}

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex BaseTreeModel::index(int row, int column, const QModelIndex& parent) const {
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	BaseTreeItem* parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<BaseTreeItem*>(parent.internalPointer());

	BaseTreeItem* childItem = parentItem->children.at(row);
	if (childItem)
		return createIndex(row, column, childItem);
	return QModelIndex();
}

QModelIndex BaseTreeModel::parent(const QModelIndex& index) const {
	if (!index.isValid())
		return QModelIndex();

	BaseTreeItem* childItem = static_cast<BaseTreeItem*>(index.internalPointer());
	BaseTreeItem* parentItem = childItem->parent;

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

QModelIndex BaseTreeModel::mapFromSource(const QModelIndex& sourceIndex) const {
	if (!sourceIndex.isValid()) {
		return {};
	}

	const std::string id = slk->index_to_row.at(sourceIndex.row());

	BaseTreeItem* parent_item = getFolderParent(id);
	for (int i = 0; i < parent_item->children.size(); i++) {
		BaseTreeItem* item = parent_item->children[i];
		if (item->id == id) {
			return createIndex(i, 0, item);
		}
	}

	return {};
}

QModelIndex BaseTreeModel::mapToSource(const QModelIndex& proxyIndex) const {
	if (!proxyIndex.isValid()) {
		return {};
	}

	BaseTreeItem* item = static_cast<BaseTreeItem*>(proxyIndex.internalPointer());

	if (item->baseCategory || item->subCategory) {
		return {};
	}

	if (slk->column_headers.contains("name")) {
		return createIndex(slk->row_headers.at(item->id), slk->column_headers.at("name"), item);
	} else {
		return createIndex(slk->row_headers.at(item->id), slk->column_headers.at("bufftip"), item);
	}

}

void BaseTreeModel::rowsInserted(const QModelIndex& parent, int first, int last) {
	assert(first == last);

	const std::string id = slk->index_to_row.at(first);
	BaseTreeItem* parent_item = getFolderParent(id);

	beginInsertRows(createIndex(parent_item->row(), 0, parent_item), parent_item->children.size(), parent_item->children.size());
	BaseTreeItem* item = new BaseTreeItem(parent_item);
	item->id = id;
	endInsertRows();
}

void BaseTreeModel::rowsRemoved(const QModelIndex& parent, int first, int last) {
	assert(first == last);

	const std::string id = slk->index_to_row.at(first);
	BaseTreeItem* parent_item = getFolderParent(id);

	int row = -1;
	for (int i = 0; i < parent_item->children.size(); i++) {
		if (parent_item->children[i]->id == id) {
			row = i;
			break;
		}
	}

	beginRemoveRows(createIndex(parent_item->row(), 0, parent_item), row, row);
	parent_item->children.remove(row);
	endRemoveRows();
}

void BaseTreeModel::setSourceModel(QAbstractItemModel* sourceModel) {
	QAbstractProxyModel::setSourceModel(sourceModel);

	connect(sourceModel, &QAbstractItemModel::dataChanged, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
		Q_ASSERT(topLeft.isValid() ? topLeft.model() == this->sourceModel() : true);
		Q_ASSERT(bottomRight.isValid() ? bottomRight.model() == this->sourceModel() : true);
		emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
	});

	connect(sourceModel, &QAbstractItemModel::rowsInserted, this, &BaseTreeModel::rowsInserted);
	connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &BaseTreeModel::rowsRemoved);
}

QVariant BaseTreeModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return {};
	}

	BaseTreeItem* item = static_cast<BaseTreeItem*>(index.internalPointer());

	switch (role) {
		case Qt::DecorationRole:
			if (item->baseCategory || item->subCategory) {
				return folderIcon;
			}
			if (slk->column_headers.contains("art")) {
				return sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("art")), role);
			} else {
				return sourceModel()->data(sourceModel()->index(slk->row_headers.at(item->id), slk->column_headers.at("buffart")), role);
			}
		case Qt::TextColorRole:
			if (item->baseCategory || item->subCategory) {
				return {};
			}

			if (slk->shadow_data.contains(item->id)) {
				return QColor("violet");
			} else {
				return {};
			}
		default:
			return {};
	}
}

bool BaseFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
	QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
	BaseTreeItem* item = static_cast<BaseTreeItem*>(index0.internalPointer());

	if (filterCustom) {
		if (item->baseCategory || item->subCategory) {
			return false;
		}

		if (!(slk->shadow_data.contains(item->id) && slk->shadow_data.at(item->id).contains("oldid"))) {
			return false;
		}
	}

	return sourceModel()->data(index0).toString().contains(filterRegExp());
}

void BaseFilter::setFilterCustom(bool filter) {
	filterCustom = filter;
	invalidateFilter();
}