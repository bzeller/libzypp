import QtQuick 2.12
import QtQuick.Controls 1.4

TreeView {
    TableViewColumn {
        title: "SolvId"
        role: "solvId"
    }
    TableViewColumn {
        title: "Name"
        role: "display"
    }

    TableViewColumn {
        title: "Repository"
        role: "repository"
    }
}

