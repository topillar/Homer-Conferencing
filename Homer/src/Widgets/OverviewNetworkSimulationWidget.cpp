/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: Implementation of OverviewNetworkSimulationWidget.h
 * Author:  Thomas Volkert
 * Since:   2012-06-03
 */

#include <Core/Coordinator.h>
#include <Core/Scenario.h>
#include <Dialogs/ContactEditDialog.h>
#include <Widgets/OverviewNetworkSimulationWidget.h>
#include <MainWindow.h>
#include <ContactsPool.h>
#include <Configuration.h>
#include <Logger.h>

#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTableWidgetItem>
#include <QStyledItemDelegate>
#include <QDockWidget>
#include <QModelIndex>
#include <QHostInfo>
#include <QPoint>
#include <QFileDialog>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class HierarchyItem:
    public QStandardItem
{
public:
    HierarchyItem(QString pText, Coordinator *pCoordinator, Node* pNode):QStandardItem()
    {
        mCoordinator = pCoordinator;
        mNode = pNode;
        setText(pText);
        //LOG(LOG_VERBOSE, "Creating hierarchy item %s with %d children", pText.toStdString().c_str(), pChildCount);
    }
    ~HierarchyItem(){ }

    Coordinator* GetCoordinator(){ return mCoordinator; }
    Node* GetNode(){ return mNode; }
private:
    Coordinator *mCoordinator;
    Node* mNode;
};

static struct StreamDescriptor sEmptyStreamDesc = {0, {0, 0, 0}, "", "", 0, 0};

class StreamItem:
    public QStandardItem
{
public:
    StreamItem(QString pText, struct StreamDescriptor pStreamDesc):QStandardItem(pText), mText(pText)
    {
        mStreamDesc = pStreamDesc;
        //LOG(LOG_VERBOSE, "Creating stream item %s", pText.toStdString().c_str());
    }
    ~StreamItem(){ }

    struct StreamDescriptor GetDesc(){ return mStreamDesc; }
    QString GetText(){ return mText; }

private:
    struct StreamDescriptor mStreamDesc;
    QString                 mText;
};

#define GUI_NODE_TYPE   (QGraphicsItem::UserType + 1)
class GuiNode:
    public QGraphicsPixmapItem
{
public:
    GuiNode(Node* pNode, QWidget* pParent):QGraphicsPixmapItem(QPixmap(":/images/46_46/Hardware.png"), NULL), mNode(pNode), mParent(pParent)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    }
    ~GuiNode()
    {

    }

    virtual int type() const
    {
        return GUI_NODE_TYPE;
    }

    Node* GetNode()
    {
        return mNode;
    }
private:
    Node        *mNode;
    QWidget*    mParent;
};

#define GUI_LINK_TYPE   (QGraphicsItem::UserType + 2)
class GuiLink:
    public QGraphicsLineItem
{
public:
    GuiLink(GuiNode* pGuiNode0, GuiNode* pGuiNode1, QWidget* pParent):QGraphicsLineItem(), mGuiNode0(pGuiNode0), mGuiNode1(pGuiNode1), mParent(pParent)
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setPen(QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        setZValue(-1000.0);
    }
    ~GuiLink()
    {

    }

    virtual int type() const
    {
        return GUI_LINK_TYPE;
    }

    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *, QWidget *)
    {
        if (mGuiNode0->collidesWithItem(mGuiNode1))
            return;

//        QPen myPen = pen();
//        myPen.setColor(Qt::black);
//        qreal arrowSize = 20;
//        pPainter->setPen(myPen);
//        pPainter->setBrush(Qt::black);
//
//        QLineF centerLine(mGuiNode0->pos(), mGuiNode1->pos());
//        QPolygonF endPolygon = mGuiNode1->polygon();
//        QPointF p1 = endPolygon.first() + mGuiNode1->pos();
//        QPointF p2;
//        QPointF intersectPoint;
//        QLineF polyLine;
//        for (int i = 1; i < endPolygon.count(); ++i)
//        {
//            p2 = endPolygon.at(i) + mGuiNode1->pos();
//            polyLine = QLineF(p1, p2);
//            QLineF::IntersectType intersectType = polyLine.intersect(centerLine, &intersectPoint);
//            if (intersectType == QLineF::BoundedIntersection)
//                break;
//            p1 = p2;
//        }
//
//        setLine(QLineF(intersectPoint, mGuiNode0->pos()));

//        if (line().dy() >= 0)
        pPainter->drawLine(line());
        if (isSelected())
        {
            pPainter->setPen(QPen(Qt::red, 1, Qt::DashLine));
            QLineF myLine = line();
            myLine.translate(0, 4.0);
            pPainter->drawLine(myLine);
            myLine.translate(0,-8.0);
            pPainter->drawLine(myLine);
        }
    }

    void UpdatePosition()
    {
        QLineF line(mapFromItem(mGuiNode0, 0, 0), mapFromItem(mGuiNode1, 0, 0));
        setLine(line);
    }

    GuiNode* GetGuiNode0()
    {
        return mGuiNode0;
    }
    GuiNode* GetGuiNode1()
    {
        return mGuiNode1;
    }
private:
    GuiNode     *mGuiNode0, *mGuiNode1;
    QWidget*    mParent;
};

class NetworkScene:
    public QGraphicsScene
{
public:
    NetworkScene(OverviewNetworkSimulationWidget *pParent): QGraphicsScene(pParent)
    {
        connect(this, SIGNAL(selectionChanged()), pParent, SLOT(SelectedNewNetworkItem()));
    }
    ~NetworkScene()
    {

    }
};

///////////////////////////////////////////////////////////////////////////////

OverviewNetworkSimulationWidget::OverviewNetworkSimulationWidget(QAction *pAssignedAction, QMainWindow* pMainWindow, Scenario *pScenario):
    QDockWidget(pMainWindow)
{
    mAssignedAction = pAssignedAction;
    mMainWindow = pMainWindow;
    mScenario = pScenario;
    mStreamRow = 0;
    mHierarchyRow = 0;
    mHierarchyCol = 0;
    mHierarchyEntry = NULL;
    mSelectedNode = NULL;

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::TopDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(true);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    connect(mTvHierarchy, SIGNAL(clicked(QModelIndex)), this, SLOT(SelectedCoordinator(QModelIndex)));
    connect(mTvStreams, SIGNAL(clicked(QModelIndex)), this, SLOT(SelectedStream(QModelIndex)));

    SetVisible(CONF.GetVisibilityNetworkSimulationWidget());
    mAssignedAction->setChecked(CONF.GetVisibilityNetworkSimulationWidget());

    InitNetworkView();
    UpdateHierarchyView();
    UpdateStreamsView();
}

OverviewNetworkSimulationWidget::~OverviewNetworkSimulationWidget()
{
    CONF.SetVisibilityNetworkSimulationWidget(isVisible());
    delete mTvHierarchyModel;
    delete mTvStreamsModel;
}

///////////////////////////////////////////////////////////////////////////////

void OverviewNetworkSimulationWidget::initializeGUI()
{
    setupUi(this);

    mTvHierarchyModel = new QStandardItemModel(this);
    mTvHierarchy->setModel(mTvHierarchyModel);
    mTvStreamsModel = new QStandardItemModel(this);
    mTvStreams->setModel(mTvStreamsModel);

    mNetworkScene = new NetworkScene(this);
    mGvNetwork->setScene(mNetworkScene);
}

void OverviewNetworkSimulationWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewNetworkSimulationWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        show();
        // update GUI elements every x ms
        mTimerId = startTimer(NETWORK_SIMULATION_GUI_UPDATE_TIME);
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        hide();
    }
}

void OverviewNetworkSimulationWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
//    QAction *tAction;
//
//    QMenu tMenu(this);
//
//    tAction = tMenu.addAction("Add contact");
//    QIcon tIcon1;
//    tIcon1.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
//    tAction->setIcon(tIcon1);
//
//    QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
//    if (tPopupRes != NULL)
//    {
//        if (tPopupRes->text().compare("Add contact") == 0)
//        {
//            InsertNew();
//            return;
//        }
//    }
}

void OverviewNetworkSimulationWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
    {
        UpdateStreamsView();
        UpdateNetworkView();
        UpdateRoutingView();
    }
}

void OverviewNetworkSimulationWidget::SelectedCoordinator(QModelIndex pIndex)
{
    if(!pIndex.isValid())
        return;

    int tCurHierarchyRow = pIndex.row();
    int tCurHierarchyCol = pIndex.column();
    void *tCurHierarchyEntry = pIndex.internalPointer();
    if (tCurHierarchyEntry != NULL)
        tCurHierarchyEntry = ((QStandardItem*)tCurHierarchyEntry)->child(tCurHierarchyRow, tCurHierarchyCol);

    HierarchyItem *tItem = (HierarchyItem*)tCurHierarchyEntry;

    if (tItem != NULL)
    {
        if (tItem->GetCoordinator() != NULL)
        {
            LOG(LOG_VERBOSE, "User selected coordinator in row: %d and col.: %d, coordinator: %s", tCurHierarchyRow, tCurHierarchyCol, tItem->GetCoordinator()->GetClusterAddress().c_str());
        }else if (tItem->GetNode() != NULL)
        {
            LOG(LOG_VERBOSE, "User selected node in row: %d and col.: %d, node: %s", tCurHierarchyRow, tCurHierarchyCol, tItem->GetNode()->GetAddress().c_str());
        }else
        {
            LOG(LOG_VERBOSE, "User selected coordinator in row: %d and col.: %d, internal pointer: %p", tCurHierarchyRow, tCurHierarchyCol, pIndex.internalPointer());
        }
    }

    if ((tCurHierarchyRow != mHierarchyRow) || (tCurHierarchyCol != mHierarchyCol) || (tCurHierarchyEntry != mHierarchyEntry))
    {
        LOG(LOG_VERBOSE, "Update of hierarchy view needed");
        mHierarchyRow = tCurHierarchyRow;
        mHierarchyCol = tCurHierarchyCol;
        mHierarchyEntry = tCurHierarchyEntry;
        ShowHierarchyDetails((tItem->GetCoordinator() != NULL) ? tItem->GetCoordinator() : NULL, tItem->GetNode() ? tItem->GetNode() : NULL);
    }
}

void OverviewNetworkSimulationWidget::SelectedStream(QModelIndex pIndex)
{
    if(!pIndex.isValid())
        return;

    int tCurStreamRow = pIndex.row();
    LOG(LOG_VERBOSE, "User selected stream in row: %d", tCurStreamRow);

    if(tCurStreamRow != mStreamRow)
    {
        LOG(LOG_VERBOSE, "Update of streams view needed");
        mStreamRow = tCurStreamRow;
    }
}

void OverviewNetworkSimulationWidget::ShowHierarchyDetails(Coordinator *pCoordinator, Node* pNode)
{
    if (pCoordinator != NULL)
    {
        mLbHierarchyLevel->setText(QString("%1").arg(pCoordinator->GetHierarchyLevel()));
        mLbSiblings->setText(QString("%1").arg(pCoordinator->GetSiblings().size()));
        if (pCoordinator->GetHierarchyLevel() != 0)
            mLbChildren->setText(QString("%1").arg(pCoordinator->GetChildCoordinators().size()));
        else
            mLbChildren->setText(QString("%1").arg(pCoordinator->GetClusterMembers().size()));
    }else
    {
        if (pNode != NULL)
        {
            mLbHierarchyLevel->setText("node");
            mLbSiblings->setText(QString("%1").arg(pNode->GetSiblings().size()));
            mLbChildren->setText("0");
        }else
        {
            mLbHierarchyLevel->setText("-");
            mLbSiblings->setText("-");
            mLbChildren->setText("-");
        }
    }
}

void OverviewNetworkSimulationWidget::UpdateHierarchyView()
{
    Coordinator *tRootCoordinator = mScenario->GetRootCoordinator();
    mTvHierarchyModel->clear();
    QStandardItem *tRootItem = mTvHierarchyModel->invisibleRootItem();

    // create root entry of the tree
    HierarchyItem *tRootCoordinatorItem = new HierarchyItem("Root: " + QString(tRootCoordinator->GetClusterAddress().c_str()), tRootCoordinator, NULL);
    tRootItem->appendRow(tRootCoordinatorItem);

    // recursive creation of tree items
    AppendHierarchySubItems(tRootCoordinatorItem);

    // show all entries of the tree
    mTvHierarchy->expandAll();

    if (mHierarchyEntry == NULL)
        ShowHierarchyDetails(NULL, NULL);
}

void OverviewNetworkSimulationWidget::AppendHierarchySubItems(HierarchyItem *pParentItem)
{
    Coordinator *tParentCoordinator = pParentItem->GetCoordinator();

    if (tParentCoordinator->GetHierarchyLevel() == 0)
    {// end of tree reached
        NodeList tNodeList = tParentCoordinator->GetClusterMembers();
        NodeList::iterator tIt;
        int tCount = 0;
        for (tIt = tNodeList.begin(); tIt != tNodeList.end(); tIt++)
        {
            HierarchyItem* tItem = new HierarchyItem("Node  " + QString((*tIt)->GetAddress().c_str()), NULL, *tIt);
            pParentItem->setChild(tCount, tItem);
            tCount++;
        }
    }else
    {// further sub trees possible
        CoordinatorList tCoordinatorList = tParentCoordinator->GetChildCoordinators();
        CoordinatorList::iterator tIt;
        QList<QStandardItem*> tItems;
        int tCount = 0;
        for (tIt = tCoordinatorList.begin(); tIt != tCoordinatorList.end(); tIt++)
        {
            HierarchyItem *tItem = new HierarchyItem("Coord. " + QString((*tIt)->GetClusterAddress().c_str()), *tIt, NULL);
            pParentItem->setChild(tCount, tItem);
            //LOG(LOG_VERBOSE, "Appending coordinator %s", tItem->GetCoordinator()->GetClusterAddress().c_str());
            AppendHierarchySubItems(tItem);
            tCount++;
        }
    }
}

// #####################################################################
// ############ streams view
// #####################################################################
void OverviewNetworkSimulationWidget::ShowStreamDetails(const struct StreamDescriptor pDesc)
{
    mLbPackets->setText(QString("%1").arg(pDesc.PacketCount));
    mLbDataRate->setText(QString("%1").arg(pDesc.QoSRequs.DataRate));
    mLbDelay->setText(QString("%1").arg(pDesc.QoSRequs.Delay));
}

QString OverviewNetworkSimulationWidget::CreateStreamId(const struct StreamDescriptor pDesc)
{
    return QString(pDesc.LocalNode.c_str()) + ":" + QString("%1").arg(pDesc.LocalPort) + " <==> " +QString(pDesc.PeerNode.c_str()) + ":" + QString("%1").arg(pDesc.PeerPort);
}

void OverviewNetworkSimulationWidget::UpdateStreamsView()
{
    bool tResetNeeded = false;

    //LOG(LOG_VERBOSE, "Updating streams view");

    StreamList tStreams = mScenario->GetStreams();
    StreamList::iterator tIt;
    QStandardItem *tRootItem = mTvStreamsModel->invisibleRootItem();

    if (tStreams.size() == 0)
    {
        ShowStreamDetails(sEmptyStreamDesc);
        // if the QTreeView is already empty we can return immediately
        if (tRootItem->rowCount() == 0)
            return;
    }

    int tCount = 0;
    // check if update is need
    if (tRootItem->rowCount() != 0)
    {
        for(tIt = tStreams.begin(); tIt != tStreams.end(); tIt++)
        {
            QString tDesiredString  = CreateStreamId(*tIt);
            StreamItem* tCurItem = (StreamItem*) mTvStreamsModel->item(tCount, 0);
            if (tCurItem != NULL)
            {
                LOG(LOG_WARN, "Comparing %s and %s", tDesiredString.toStdString().c_str(), tCurItem->GetText().toStdString().c_str());
                if (tDesiredString != tCurItem->GetText())
                    tResetNeeded = true;
            }else
                tResetNeeded = true;
            if (tCount == mStreamRow)
                ShowStreamDetails(*tIt);
            tCount++;
        }
    }else
        tResetNeeded = true;

    // update the entire view
    if (tResetNeeded)
    {
        tCount = 0;
        mTvStreamsModel->clear();
        tRootItem = mTvStreamsModel->invisibleRootItem();
        if (tRootItem == NULL)
        {
            LOG(LOG_WARN, "Root item is invalid");
            tRootItem = new QStandardItem();
            mTvStreamsModel->appendRow(tRootItem);
        }
        for(tIt = tStreams.begin(); tIt != tStreams.end(); tIt++)
        {
            StreamItem* tItem = new StreamItem(CreateStreamId(*tIt), *tIt);
            tRootItem->appendRow(tItem);
            if (tCount == mStreamRow)
                ShowStreamDetails(*tIt);
            tCount++;
        }
    }
}

// #####################################################################
// ############ network view
// #####################################################################
GuiNode* OverviewNetworkSimulationWidget::GetGuiNode(Node *pNode)
{
    GuiNode *tGuiNode;
    foreach(tGuiNode, mGuiNodes)
    {
        if (tGuiNode->GetNode()->GetAddress() == pNode->GetAddress())
        {
            return tGuiNode;
        }
    }

    LOG(LOG_ERROR, "Cannot determine GuiNode object for the given node %s", pNode->GetAddress().c_str());

    return NULL;
}

void OverviewNetworkSimulationWidget::InitNetworkView()
{
    // create all node GUI elements
    NodeList tNodes = mScenario->GetNodes();
    NodeList::iterator tIt;
    for (tIt = tNodes.begin(); tIt != tNodes.end(); tIt++)
    {
        GuiNode *tGuiNode = new GuiNode(*tIt, this);
        mGuiNodes.append(tGuiNode);
        mNetworkScene->addItem(tGuiNode);
        tGuiNode->setPos((*tIt)->GetPosXHint(), (*tIt)->GetPosYHint());
        LOG(LOG_VERBOSE, "Set pos. of node %s to %d,%d", (*tIt)->GetAddress().c_str(), (*tIt)->GetPosXHint(), (*tIt)->GetPosYHint());
        QGraphicsTextItem *tText = new QGraphicsTextItem(QString((*tIt)->GetAddress().c_str()), tGuiNode);
        tText->setPos(0, 45);
    }

    // create all link GUI elements
    LinkList tLinks = mScenario->GetLinks();
    LinkList::iterator tIt2;
    for (tIt2 = tLinks.begin(); tIt2 != tLinks.end(); tIt2++)
    {
        GuiLink *tGuiLink = new GuiLink(GetGuiNode((*tIt2)->GetNode0()), GetGuiNode((*tIt2)->GetNode1()), this);
        mNetworkScene->addItem(tGuiLink);
    }
}

void OverviewNetworkSimulationWidget::UpdateNetworkView()
{
    GuiLink *tGuiLink;
    foreach(tGuiLink, mGuiLinks)
    {
        tGuiLink->UpdatePosition();
    }
}

// #####################################################################
// ############ routing view
// #####################################################################
void OverviewNetworkSimulationWidget::SelectedNewNetworkItem()
{
    QList<QGraphicsItem*> tItems = mNetworkScene->selectedItems();
    QList<QGraphicsItem*>::iterator tIt;
    for (tIt = tItems.begin(); tIt != tItems.end(); tIt++)
    {
        // our way of "reflections": show routing table of first selected node
        if ((*tIt)->type() == GUI_NODE_TYPE)
        {
            LOG(LOG_VERBOSE, "New node item selected");
            GuiNode *tSelectedGuiNode = (GuiNode*)tItems.first();
            mSelectedNode = tSelectedGuiNode->GetNode();
            break;
        }
        if ((*tIt)->type() == GUI_LINK_TYPE)
        {
            LOG(LOG_VERBOSE, "New link item selected");
        }
    }
}

void OverviewNetworkSimulationWidget::FillRoutingTableCell(int pRow, int pCol, QString pText)
{
    if (mTwRouting->item(pRow, pCol) != NULL)
        mTwRouting->item(pRow, pCol)->setText(pText);
    else
    {
        QTableWidgetItem *tItem =  new QTableWidgetItem(pText);
        tItem->setTextAlignment(Qt::AlignCenter|Qt::AlignVCenter);
        mTwRouting->setItem(pRow, pCol, tItem);
    }
}

void OverviewNetworkSimulationWidget::FillRoutingTableRow(int pRow, RibEntry* pEntry)
{
    if (pRow > mTwRouting->rowCount() - 1)
        mTwRouting->insertRow(mTwRouting->rowCount());

    FillRoutingTableCell(pRow, 0, QString(pEntry->Destination.c_str()));
    FillRoutingTableCell(pRow, 1, QString(pEntry->NextNode.c_str()));
    FillRoutingTableCell(pRow, 2, QString("%1").arg(pEntry->HopCount));
    FillRoutingTableCell(pRow, 3, QString("%1").arg(pEntry->QoSCapabilities.DataRate));
    FillRoutingTableCell(pRow, 4, QString("%1").arg(pEntry->QoSCapabilities.Delay));
}

void OverviewNetworkSimulationWidget::UpdateRoutingView()
{
    NodeList tNodes = mScenario->GetNodes();

    if ((mSelectedNode == NULL) && (tNodes.size() > 0))
    {
        // select first in list
        mSelectedNode = *tNodes.begin();
    }

    if (mSelectedNode == NULL)
        return;

    mGrpRouting->setTitle(" Routing table " + QString(mSelectedNode->GetAddress().c_str()));
    RibTable tRib = mSelectedNode->GetRib();
    RibTable::iterator tIt;
    int tRow = 0;
    for (tIt = tRib.begin(); tIt != tRib.end(); tIt++)
    {
        FillRoutingTableRow(tRow++, *tIt);
    }

    for (int i = mTwRouting->rowCount(); i > tRow; i--)
        mTwRouting->removeRow(i);

    mTwRouting->setRowCount(tRow);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
