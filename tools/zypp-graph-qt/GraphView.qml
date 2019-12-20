import QtQuick 2.9
import QtQuick.Controls 2.12
import QtQuick.Shapes 1.13
import de.suse.zyppgraph 1.0

Flickable {
    id: scrollview
    clip: true

    contentHeight: graphView.height
    contentWidth:  graphView.width

    property alias model: graphView.model
    property alias zoom: graphView.zoom
    property var graphItem: graphView
    property int selectedSolvId: -1

    ScrollBar.vertical: ScrollBar { }
    ScrollBar.horizontal: ScrollBar { }

    FocusScope {
        GraphItem {
            id: graphView
            //@TODO add intialWidth and height to have a reference for the unscaled size
            width: scrollview.width
            height: scrollview.height
            focus: false

            onSelectedItemChanged: {
                triggerLayout();
            }

            nodeDelegate: Rectangle {
                    id: container
                    x: 0
                    y: 0
                    z: 10
                    Behavior on x { PropertyAnimation { duration: 2000; easing.type: Easing.OutElastic; } }
                    Behavior on y { PropertyAnimation { duration: 2000; easing.type: Easing.OutElastic; } }
                    width:  50; height: 15
                    //radius: 10
                    border.width: 1
                    focus: true
                    color:  MyGraphItem.selectedItem === container ? "red" : "yellow"

                    Text {
                        fontSizeMode: Text.Fit
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: solvId
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            MyGraphItem.doSelectItem( container );
                            scrollview.selectedSolvId = solvId;
                        }
                    }
                }

            lineDelegate: Shape {
                z: 0
                 ShapePath {
                    strokeColor: MyGraphItem.selectedItem == sourceNode || MyGraphItem.selectedItem == targetNode  ? "red" : "light grey"
                    startX: sourceNode.x + sourceNode.radius
                    startY: sourceNode.y + sourceNode.radius
                    PathLine {
                        x: targetNode.x + targetNode.radius
                        y: targetNode.y + targetNode.radius
                    }

                }
            }
        }
    }
}
