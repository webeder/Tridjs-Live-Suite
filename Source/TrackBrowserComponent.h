#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include "TrackDatabase.h"
#include "AnalysisManager.h"

class TrackBrowserComponent : public juce::Component,
                             public juce::TableListBoxModel,
                             public juce::FileDragAndDropTarget,
                             public juce::DragAndDropTarget,
                             public juce::Timer,
                             public juce::KeyListener
{
public:
    TrackBrowserComponent(TrackDatabase& db, AnalysisManager& am);
    ~TrackBrowserComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void sortOrderChanged(int newSortColumnId, bool isAscending) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    // FileDragAndDropTarget (External)
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // DragAndDropTarget (Internal)
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void refresh();
    void loadCollection();
    void loadPlaylist(int playlistId);
    void loadFolder(const juce::File& folder, bool recursive = false);
    
    void updateTitle();
    juce::Label titleLabel;

    std::function<void(const juce::File&)> onTrackDoubleClicked;
    
    enum class ViewType { Collection, Playlist, Folder, Recordings };
    ViewType currentView = ViewType::Collection;
    int activePlaylistId = -1; 
    
    void addFilesToPlaylist(const juce::StringArray& files, int playlistId);
    void removeSelectedTracksFromPlaylist();
    void deleteSelectedTracksFromDisk();
    void deleteSelectedTracksFromCollection();

private:
    void updateTableColumns();
    
    TrackDatabase& database;
    AnalysisManager& analysisManager;
    std::vector<TrackDatabase::Track> tracks;

    juce::String currentSortColumn = "name";
    bool sortAscending = true;
    
    juce::TableListBox table;
    juce::TextEditor searchBox;
    juce::TextButton resetButton;
    
    // Sidebar TreeView
    juce::TreeView sidebarTree;
    std::unique_ptr<juce::TreeViewItem> rootItem;

    void drawRating(juce::Graphics& g, int rating, int x, int y, int w, int h);
    void importFiles(const juce::StringArray& paths);

    // Tree Item Classes
    class RootSidebarItem : public juce::TreeViewItem {
    public:
        RootSidebarItem() {}
        bool mightContainSubItems() override { return true; }
        void paintItem(juce::Graphics&, int, int) override {}
    };

    class SidebarItem : public juce::TreeViewItem {
    public:
        SidebarItem(TrackBrowserComponent& owner, const juce::String& name, bool expandable) 
            : owner(owner), name(name), isExpandable(expandable) {}
        
        bool mightContainSubItems() override { return isExpandable; }
        void paintItem(juce::Graphics& g, int width, int height) override;
        void itemClicked(const juce::MouseEvent& e) override;
        
        bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
        void itemDropped (const juce::DragAndDropTarget::SourceDetails& details, int insertIndex) override;
        juce::var getDragSourceDescription() override;
        
        juce::String name;
        bool isExpandable;
        TrackBrowserComponent& owner;
    };

    class PlaylistRootItem : public SidebarItem {
    public:
        PlaylistRootItem(TrackBrowserComponent& owner) : SidebarItem(owner, "PLAYLISTS", true) {}
        void itemOpennessChanged(bool isNowOpen) override;
        void itemClicked(const juce::MouseEvent& e) override;
        bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
        void itemDropped(const juce::DragAndDropTarget::SourceDetails& details, int insertIndex) override;
    };

    class PlaylistItem : public SidebarItem {
    public:
        PlaylistItem(TrackBrowserComponent& owner, int id, const juce::String& name) 
            : SidebarItem(owner, name, false), playlistId(id) {}
        
        void paintItem(juce::Graphics& g, int width, int height) override;
        void itemClicked(const juce::MouseEvent& e) override;
        void itemDoubleClicked(const juce::MouseEvent& e) override;
        bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
        void itemDropped(const juce::DragAndDropTarget::SourceDetails& details, int insertIndex) override;
        int playlistId;
    };

    class ExplorerRootItem : public SidebarItem {
    public:
        ExplorerRootItem(TrackBrowserComponent& owner) : SidebarItem(owner, "EXPLORER", true) {}
        void itemOpennessChanged(bool isNowOpen) override;
    };

    class FileItem : public SidebarItem {
    public:
        FileItem(TrackBrowserComponent& owner, const juce::File& file) 
            : SidebarItem(owner, file.getFileName(), file.isDirectory()), file(file) {}
        
        void itemOpennessChanged(bool isNowOpen) override;
        void itemClicked(const juce::MouseEvent& e) override;
        juce::var getDragSourceDescription() override;
        juce::File file;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackBrowserComponent)
};
