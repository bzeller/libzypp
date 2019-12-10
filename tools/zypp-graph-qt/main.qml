import QtQuick 2.9
import QtQuick.Controls 2.12
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.0
import QtQuick.Layouts 1.12
import Qt.labs.platform 1.0 as LabsTypes
import de.suse.zyppgraph 1.0


ApplicationWindow {
    id: window
    visible: true
    width: 1024
    height: 768
    title: qsTr("ZyppGraph")

    Component.onCompleted: {
        x = Screen.width / 2 - width / 2
        y = Screen.height / 2 - height / 2
    }

    Shortcut {
        sequence: StandardKey.Open
        onActivated: openDialog.open()
    }
    Shortcut {
        sequence: StandardKey.Quit
        onActivated: close()

    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")

            MenuItem {
                text: qsTr("&Open Testcase")
                onTriggered: openDialog.open()
            }
            MenuItem {
                text: qsTr("&Open System")
                onTriggered: openDialog.open()
            }
            MenuItem {
                text: qsTr("&Quit")
                onTriggered: close()
            }
        }
    }

    header: ToolBar {
        leftPadding: 8


            Flow {
                id: flow
                width: parent.width

                Row {
                    id: fileRow
                    ToolButton {
                        id: openButton
                        text: "\uF115" // icon-folder-open-empty
                        font.family: "FontAwesome"
                        onClicked: openDialog.open()
                    }

                    ToolSeparator {
                        contentItem.visible: fileRow.y === toolsRow.y
                    }
                }

                Row {
                    id: toolsRow
                    ToolButton {
                        id: scaleUpBtn
                        text: "\uf00e"
                        onClicked: {
                            graphView.zoom = graphView.zoom + 1;
                        }
                    }
                    ToolButton {
                        id: scaleDwnBtn
                        text: "\uf010"
                        onClicked: {
                            graphView.zoom = graphView.zoom - 1;
                        }
                    }
                }
            }
    }

    LabsTypes.FolderDialog {
        id: openDialog
        title: "Please choose a testcase directory"
        folder: shortcuts.home
        onAccepted: {
            pool.load( openDialog.folder )
        }
        onRejected: {
            console.log("Canceled")
        }
        Component.onCompleted: visible = true
    }

    ZyppPool {
        id: pool
    }

    ListModel {
        id: availableRolesModel

        ListElement {
            name: "None"
            role: 0
        }
        ListElement {
            name: "SolvId"
            role: 0x0100
        }
        ListElement {
            name: "Name"
            role: 0
        }
        ListElement {
            name: "Repository"
            role: 0x0101
        }
    }

    SortModel {
        id: filterModel
        sourceModel: pool
        filterRole: {
            return filters.currentIndex > 0 ? availableRolesModel.get( filters.currentIndex ).role : 0;
        }
    }

    ColumnLayout {
        anchors.fill: parent

         RowLayout{
            ComboBox {
                id: filters
                textRole: "name"
                model: availableRolesModel
            }
            TextField {
                id: filterText
                placeholderText: "Enter text to filter"
                enabled: filters.currentIndex > 0
                Layout.fillWidth: true
                onTextChanged: filterModel.setFilterFixedString( filterText.text )
                onEnabledChanged: {
                    if ( !enabled ) filterText.clear();
                }
            }
        }

        RowLayout {
            GraphView {
                id: graphView
                Layout.fillWidth: true
                Layout.fillHeight: true

            }

            TreeWidget {
                model: filterModel
                Layout.fillHeight: true

                onActivated: {
                    console.log("Selected solvId: " +  filterModel.data( index, 0x0100 ));
                    graphView.graphItem.showDependencyGraphFor( filterModel.data( index, 0x0100 ), 10 );
                }
            }
        }
    }


    /*
    Item {
        anchors.fill: parent
        Graph {
            id: graphModel
            Component.onCompleted: load("/home/zbenjamin/workspace/testcase/1124069/zypper.solverTestCase")
        }

        GraphView {
            id: graphView
            anchors.fill: parent
            model: graphModel
        }
    }
    */
}
