#include "TrackBrowserComponent.h"
#include "AnalysisManager.h"
#include "TrackDatabase.h"

TrackBrowserComponent::TrackBrowserComponent(TrackDatabase &db,
                                             AnalysisManager &am)
    : database(db), analysisManager(am) {
  addAndMakeVisible(titleLabel);
  titleLabel.setText("COLLECTION", juce::dontSendNotification);
  titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);

  addAndMakeVisible(table);
  table.setModel(this);
  table.getHeader().addColumn("#", 1, 40);
  table.getHeader().addColumn("TRACK NAME", 2, 350, 100, -1);
  table.getHeader().addColumn("ARTIST",     3, 200, 100, -1);
  table.getHeader().addColumn("BPM",        4,  80,  50, -1);
  table.getHeader().addColumn("KEY",        5,  80,  50, -1);
  table.getHeader().addColumn("RATING",     6, 120,  80, -1);
  table.getHeader().addColumn("COMMENT",    7, 300, 100, -1);

  table.getHeader().setSortColumnId(2, true); // Sort by Name by default
  currentSortColumn = "TRACK NAME";
  sortAscending = true;

  table.setColour(juce::ListBox::backgroundColourId,
                  juce::Colours::transparentBlack);
  table.setColour(juce::ListBox::outlineColourId,
                  juce::Colours::transparentBlack);

  addAndMakeVisible(searchBox);
  searchBox.setTextToShowWhenEmpty("SEARCH TRACKS...", juce::Colours::grey);
  searchBox.onTextChange = [this] {
    if (currentView == ViewType::Collection)
      loadCollection();
    else if (currentView == ViewType::Playlist)
      loadPlaylist(activePlaylistId);
    else
      refresh(); // Folders handle filtering in refresh or paintRow
  };
  searchBox.setColour(juce::TextEditor::backgroundColourId,
                      juce::Colour(0xff121212));
  searchBox.setColour(juce::TextEditor::outlineColourId,
                      juce::Colours::transparentBlack);

  addAndMakeVisible(resetButton);
  resetButton.setButtonText("RESET");
  resetButton.setTooltip("Clear filters and sorting");
  resetButton.onClick = [this] {
    searchBox.clear();
    currentSortColumn = "name";
    sortAscending = true;
    table.getHeader().setSortColumnId(2, true);
    if (currentView == ViewType::Collection)
      loadCollection();
    else if (currentView == ViewType::Playlist)
      loadPlaylist(activePlaylistId);
    else
      refresh();
  };
  resetButton.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff222222));
  resetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);

  addAndMakeVisible(sidebarTree);
  sidebarTree.setColour(juce::TreeView::backgroundColourId,
                        juce::Colours::transparentBlack);
  sidebarTree.setDefaultOpenness(true);

  rootItem = std::make_unique<RootSidebarItem>();

  auto collection = std::make_unique<SidebarItem>(*this, "COLLECTION", false);
  rootItem->addSubItem(collection.release());

  rootItem->addSubItem(new PlaylistRootItem(*this));
  rootItem->addSubItem(new ExplorerRootItem(*this));

  auto recordings = std::make_unique<SidebarItem>(*this, "RECORDINGS", false);
  rootItem->addSubItem(recordings.release());

  sidebarTree.setRootItem(rootItem.get());
  sidebarTree.setRootItemVisible(false);

  analysisManager.onAnalysisFinished = [this] { refresh(); };

  table.addKeyListener(this);

  loadCollection();
  startTimer(500);
}

TrackBrowserComponent::~TrackBrowserComponent() {
  table.removeKeyListener(this);
  sidebarTree.setRootItem(nullptr);
  stopTimer();
}

void TrackBrowserComponent::refresh() {
  table.updateContent();
  repaint();
}

void TrackBrowserComponent::loadCollection() {
  currentView = ViewType::Collection;
  activePlaylistId = -1;
  tracks.clear();
  database.searchTracks(searchBox.getText(), tracks, currentSortColumn,
                        sortAscending);
  refresh();
  updateTitle();
}

void TrackBrowserComponent::loadPlaylist(int playlistId) {
  currentView = ViewType::Playlist;
  activePlaylistId = playlistId;
  tracks.clear();
  database.getTracksInPlaylist(playlistId, tracks);

  // Filter results if search is active
  juce::String filter = searchBox.getText().trim().toLowerCase();
  if (filter.isNotEmpty()) {
    tracks.erase(
        std::remove_if(tracks.begin(), tracks.end(),
                       [&](const TrackDatabase::Track &t) {
                         return !t.name.toLowerCase().contains(filter) &&
                                !t.artist.toLowerCase().contains(filter);
                       }),
        tracks.end());
  }

  refresh();
  updateTitle();
}

void TrackBrowserComponent::loadFolder(const juce::File &folder,
                                       bool recursive) {
  currentView = folder.getFullPathName().contains("tridjs_lifeStudio")
                    ? ViewType::Recordings
                    : ViewType::Folder;
  activePlaylistId = -1;
  tracks.clear();
  juce::Array<juce::File> files;
  folder.findChildFiles(files, juce::File::findFiles, recursive,
                        "*.mp3;*.wav;*.flac;*.m4a");

  for (const auto &f : files) {
    TrackDatabase::Track t;
    if (database.getTrackByPath(f.getFullPathName(), t)) {
      tracks.push_back(t);
    } else {
      t.path = f.getFullPathName();
      t.name = f.getFileNameWithoutExtension();
      tracks.push_back(t);
    }
  }

  // Filter results if search is active
  juce::String filter = searchBox.getText().trim().toLowerCase();
  if (filter.isNotEmpty()) {
    tracks.erase(
        std::remove_if(tracks.begin(), tracks.end(),
                       [&](const TrackDatabase::Track &t) {
                         return !t.name.toLowerCase().contains(filter) &&
                                !t.artist.toLowerCase().contains(filter);
                       }),
        tracks.end());
  }

  refresh();
  updateTitle();
}

void TrackBrowserComponent::updateTitle() {
  if (currentView == ViewType::Collection)
    titleLabel.setText("COLLECTION", juce::dontSendNotification);
  else if (currentView == ViewType::Recordings)
    titleLabel.setText("RECORDINGS", juce::dontSendNotification);
  else if (currentView == ViewType::Playlist) {
    std::vector<TrackDatabase::Playlist> playlists;
    database.getAllPlaylists(playlists);
    for (const auto &p : playlists) {
      if (p.id == activePlaylistId) {
        titleLabel.setText("PLAYLIST: " + p.name.toUpperCase(),
                           juce::dontSendNotification);
        break;
      }
    }
  } else
    titleLabel.setText("EXPLORER", juce::dontSendNotification);
}

void TrackBrowserComponent::timerCallback() { table.repaint(); }

bool TrackBrowserComponent::keyPressed(const juce::KeyPress &key,
                                       juce::Component *) {
  if (key.isKeyCode(juce::KeyPress::deleteKey)) {
    if (currentView == ViewType::Playlist) {
      removeSelectedTracksFromPlaylist();
      return true;
    } else if (currentView == ViewType::Recordings) {
      deleteSelectedTracksFromDisk();
      return true;
    } else if (currentView == ViewType::Collection) {
      deleteSelectedTracksFromCollection();
      return true;
    }
  }
  return false;
}

void TrackBrowserComponent::deleteSelectedTracksFromCollection() {
  auto selectedRows = table.getSelectedRows();
  if (selectedRows.size() == 0)
    return;

  juce::AlertWindow::showOkCancelBox(
      juce::AlertWindow::QuestionIcon, "Delete from Collection",
      "Are you sure you want to remove " + juce::String(selectedRows.size()) +
          " track(s) from your collection permanently?",
      "Delete", "Cancel", nullptr,
      juce::ModalCallbackFunction::create([this, selectedRows](int result) {
        if (result != 0) {
          for (int i = 0; i < selectedRows.size(); ++i) {
            int row = selectedRows[i];
            if (row >= 0 && row < (int)tracks.size()) {
              database.removeTrackFromCollection(tracks[row].id);
            }
          }
          loadCollection();
        }
      }));
}

void TrackBrowserComponent::deleteSelectedTracksFromDisk() {
  auto selectedRows = table.getSelectedRows();
  if (selectedRows.size() == 0)
    return;

  juce::AlertWindow::showOkCancelBox(
      juce::AlertWindow::QuestionIcon, "Delete Files",
      "Are you sure you want to PERMANENTLY delete " +
          juce::String(selectedRows.size()) + " file(s) from disk?",
      "Delete", "Cancel", nullptr,
      juce::ModalCallbackFunction::create([this, selectedRows](int result) {
        if (result != 0) {
          for (int i = 0; i < selectedRows.size(); ++i) {
            int row = selectedRows[i];
            if (row >= 0 && row < (int)tracks.size()) {
              juce::File f(tracks[row].path);
              if (f.existsAsFile())
                f.deleteFile();
            }
          }
          refresh(); // Refresh folder view
        }
      }));
}

void TrackBrowserComponent::removeSelectedTracksFromPlaylist() {
  auto selectedRows = table.getSelectedRows();
  if (selectedRows.size() == 0)
    return;

  for (int i = 0; i < selectedRows.size(); ++i) {
    int row = selectedRows[i];
    if (row >= 0 && row < (int)tracks.size()) {
      database.removeTrackEntry(tracks[row].playlistEntryId);
    }
  }
  loadPlaylist(activePlaylistId);
}

void TrackBrowserComponent::paint(juce::Graphics &g) {
  auto area = getLocalBounds();
  g.fillAll(juce::Colour(0xff0a0a0a));
  auto sidebarArea = area.removeFromLeft(180);
  g.setColour(juce::Colour(0xff121212));
  g.fillRect(sidebarArea);
  g.setColour(juce::Colours::cyan);
  g.setFont(juce::Font(14.0f, juce::Font::bold));
  g.drawText("BROWSER", sidebarArea.removeFromTop(40).reduced(10, 0),
             juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff222222));
  g.drawVerticalLine(180, 0, (float)getHeight());
}

void TrackBrowserComponent::resized() {
  auto area = getLocalBounds();
  auto sidebarArea = area.removeFromLeft(180);
  sidebarTree.setBounds(sidebarArea.withTrimmedTop(40));

  auto headerArea = area.removeFromTop(45).reduced(10, 8);
  searchBox.setBounds(headerArea.removeFromLeft(300));
  headerArea.removeFromLeft(10);
  resetButton.setBounds(headerArea.removeFromLeft(60));

  titleLabel.setBounds(headerArea.removeFromRight(200));

  table.setBounds(area);
}

int TrackBrowserComponent::getNumRows() { return (int)tracks.size(); }

void TrackBrowserComponent::paintRowBackground(juce::Graphics &g, int, int, int,
                                               bool rowIsSelected) {
  if (rowIsSelected) {
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillAll();
  }
}

void TrackBrowserComponent::paintCell(juce::Graphics &g, int rowNumber,
                                      int columnId, int width, int height,
                                      bool rowIsSelected) {
  if (rowNumber >= tracks.size())
    return;
  const auto &t = tracks[rowNumber];
  g.setColour(rowIsSelected ? juce::Colour(0xff00ff55) : juce::Colours::white);
  g.setFont(
      juce::Font(13.0f, rowIsSelected ? juce::Font::bold : juce::Font::plain));
  auto textArea = juce::Rectangle<int>(width, height).reduced(8, 0);
  switch (columnId) {
  case 1:
    g.drawText(juce::String(rowNumber + 1), textArea,
               juce::Justification::centredLeft);
    break;
  case 2:
    g.drawText(t.name, textArea, juce::Justification::centredLeft);
    break;
  case 3:
    g.drawText(t.artist, textArea, juce::Justification::centredLeft);
    break;
  case 4: {
    float progress = analysisManager.getAnalysisProgress(t.path);
    if (progress >= 0.0f) {
      g.setColour(juce::Colours::cyan);
      int percent = (int)(progress * 100.0f);
      g.drawText("ANALISANDO " + juce::String(percent) + "%", textArea,
                 juce::Justification::centredLeft);
    } else {
      g.drawText(t.bpm > 0 ? juce::String(t.bpm, 1) : "---", textArea,
                 juce::Justification::centredLeft);
    }
    break;
  }
  case 5:
    g.drawText(t.key.isNotEmpty() ? t.key : "---", textArea,
               juce::Justification::centredLeft);
    break;
  case 6:
    drawRating(g, t.rating, 8, 0, width - 16, height);
    break;
  case 7: {
    if (t.comment.isNotEmpty()) {
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.drawText(t.comment, textArea, juce::Justification::centredLeft);
    } else {
      g.setColour(juce::Colours::grey.withAlpha(0.3f));
      g.drawText("Double-click to comment...", textArea,
                 juce::Justification::centredLeft);
    }
    break;
  }
  }
  g.setColour(juce::Colour(0xff222222));
  g.drawHorizontalLine(height - 1, 0, (float)width);
}

void TrackBrowserComponent::cellClicked(int rowNumber, int columnId,
                                        const juce::MouseEvent &e) {
  if (rowNumber >= (int)tracks.size())
    return;
  table.selectRow(rowNumber);

  // Rating Click Logic
  if (columnId == 6) {
    // e.x is relative to the entire row, not the cell.
    // Compute cellStartX by summing widths of all columns before this one.
    int cellStartX = 0;
    for (int i = 0; i < table.getHeader().getNumColumns(true); ++i) {
      int cId = table.getHeader().getColumnIdOfIndex(i, true);
      if (cId == columnId) break;
      cellStartX += table.getHeader().getColumnWidth(cId);
    }
    float xInCell = (float)(e.x - cellStartX) - 8.0f; // 8px = cell padding

    // Each circle is 20px wide (matches drawRating: i * 20.0f)
    int newRating = juce::jlimit(1, 5, (int)(xInCell / 20.0f) + 1);

    tracks[rowNumber].rating = newRating;
    database.updateRating(tracks[rowNumber].id, newRating);
    table.repaint();
  }

  if (e.mods.isRightButtonDown()) {
    juce::PopupMenu m;
    if (currentView == ViewType::Playlist) {
      m.addItem(1, "Remove from Playlist");
    } else if (currentView == ViewType::Collection) {
      m.addItem(2, "Delete from Collection");
    } else if (currentView == ViewType::Recordings) {
      m.addItem(3, "Delete File from Disk");
    }

    m.showMenuAsync(juce::PopupMenu::Options(), [this](int res) {
      if (res == 1)
        removeSelectedTracksFromPlaylist();
      else if (res == 2)
        deleteSelectedTracksFromCollection();
      else if (res == 3)
        deleteSelectedTracksFromDisk();
    });
  }
}

void TrackBrowserComponent::cellDoubleClicked(int rowNumber, int columnId,
                                              const juce::MouseEvent &e) {
  if (rowNumber >= (int)tracks.size())
    return;

  if (columnId == 7) {
    // Comment Dialog - Mais limpo
    auto *aw = new juce::AlertWindow(
        "TRACK COMMENT", "Enter notes for: " + tracks[rowNumber].name,
        juce::AlertWindow::NoIcon);
    aw->addTextEditor("comment", tracks[rowNumber].comment, "");
    aw->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(
        true,
        juce::ModalCallbackFunction::create([this, rowNumber, aw](int res) {
          if (res == 1) {
            juce::String newComment = aw->getTextEditorContents("comment");
            tracks[rowNumber].comment = newComment;
            database.updateComment(tracks[rowNumber].id, newComment);
            table.repaint();
          }
        }),
        true);
  } else {
    if (onTrackDoubleClicked)
      onTrackDoubleClicked(juce::File(tracks[rowNumber].path));
  }
}

void TrackBrowserComponent::sortOrderChanged(int newSortColumnId,
                                             bool isAscending) {
  juce::String colName = table.getHeader().getColumnName(newSortColumnId);

  // Mapear para nomes de colunas do banco
  if (colName == "TRACK NAME")
    currentSortColumn = "name";
  else if (colName == "ARTIST")
    currentSortColumn = "artist";
  else if (colName == "BPM")
    currentSortColumn = "bpm";
  else if (colName == "KEY")
    currentSortColumn = "key";
  else if (colName == "RATING")
    currentSortColumn = "rating";
  else if (colName == "COMMENT")
    currentSortColumn = "comment";
  else
    currentSortColumn = "name";

  sortAscending = isAscending;

  if (currentView == ViewType::Collection)
    loadCollection();
  else {
    // Sort the local vector for other views
    std::sort(
        tracks.begin(), tracks.end(),
        [&](const TrackDatabase::Track &a, const TrackDatabase::Track &b) {
          juce::String valA, valB;
          if (newSortColumnId == 2) {
            valA = a.name;
            valB = b.name;
          } else if (newSortColumnId == 3) {
            valA = a.artist;
            valB = b.artist;
          } else if (newSortColumnId == 4) {
            if (a.bpm == b.bpm)
              return false;
            return isAscending ? a.bpm < b.bpm : a.bpm > b.bpm;
          } else if (newSortColumnId == 5) {
            valA = a.key;
            valB = b.key;
          } else if (newSortColumnId == 6) {
            if (a.rating == b.rating)
              return false;
            return isAscending ? a.rating < b.rating : a.rating > b.rating;
          } else if (newSortColumnId == 7) {
            valA = a.comment;
            valB = b.comment;
          }

          return isAscending ? valA.compareNatural(valB) < 0
                             : valA.compareNatural(valB) > 0;
        });
    refresh();
  }
}

juce::var TrackBrowserComponent::getDragSourceDescription(
    const juce::SparseSet<int> &selectedRows) {
  juce::StringArray paths;
  for (int i = 0; i < selectedRows.size(); ++i) {
    int row = selectedRows[i];
    if (row >= 0 && row < (int)tracks.size())
      paths.add(tracks[row].path);
  }
  return paths.joinIntoString("|");
}
bool TrackBrowserComponent::isInterestedInFileDrag(const juce::StringArray &) {
  return true;
}
void TrackBrowserComponent::filesDropped(const juce::StringArray &files, int x,
                                         int y) {
  auto *item = sidebarTree.getItemAt(y - sidebarTree.getY());
  if (auto *pi = dynamic_cast<PlaylistItem *>(item)) {
    addFilesToPlaylist(files, pi->playlistId);
  } else {
    importFiles(files);
  }
}

bool TrackBrowserComponent::isInterestedInDragSource(const SourceDetails &) {
  return true;
}

void TrackBrowserComponent::itemDropped(const SourceDetails &d) {
  auto *item = sidebarTree.getItemAt(d.localPosition.y - sidebarTree.getY());
  if (auto *pi = dynamic_cast<PlaylistItem *>(item)) {
    juce::StringArray paths;
    paths.addTokens(d.description.toString(), "|", "");
    addFilesToPlaylist(paths, pi->playlistId);
  } else {
    juce::StringArray paths;
    paths.addTokens(d.description.toString(), "|", "");
    importFiles(paths);
  }
}

void TrackBrowserComponent::addFilesToPlaylist(const juce::StringArray &paths,
                                               int playlistId) {
  juce::Component::SafePointer<TrackBrowserComponent> safeThis(this);

  juce::Thread::launch([safeThis, paths, playlistId]() {
    if (safeThis == nullptr)
      return;

    auto &db = safeThis->database;
    db.beginTransaction();

    std::function<void(const juce::StringArray &)> processFiles =
        [&](const juce::StringArray &p) {
          for (const auto &path : p) {
            if (path.isEmpty())
              continue;
            juce::File f(path);
            if (f.isDirectory()) {
              juce::Array<juce::File> subFiles;
              f.findChildFiles(subFiles, juce::File::findFiles, true,
                               "*.mp3;*.wav;*.flac;*.m4a");
              juce::StringArray subPaths;
              for (const auto &sf : subFiles)
                subPaths.add(sf.getFullPathName());
              processFiles(subPaths);
            } else if (f.existsAsFile()) {
              TrackDatabase::Track t;
              int trackId = 0;
              if (db.getTrackByPath(f.getFullPathName(), t)) {
                trackId = t.id;
              } else {
                t.path = f.getFullPathName();
                t.name = f.getFileNameWithoutExtension();
                t.createdAt = juce::Time::getCurrentTime();
                t.lastPlayed = t.createdAt;
                trackId = db.addOrUpdateTrack(t);
              }

              if (trackId > 0) {
                // Bulk imports (folders or multiple selection) skip the
                // duplicate prompt
                if (paths.size() > 1 || f.isDirectory()) {
                  db.addTrackToPlaylist(playlistId, trackId);
                } else {
                  // Single file: check duplicate
                  if (db.isTrackInPlaylist(playlistId, trackId)) {
                    juce::MessageManager::callAsync([safeThis, playlistId,
                                                     trackId]() {
                      if (safeThis == nullptr)
                        return;
                      juce::AlertWindow::showOkCancelBox(
                          juce::AlertWindow::QuestionIcon, "Duplicate Track",
                          "This track is already in the playlist. Add anyway?",
                          "Yes", "No", nullptr,
                          juce::ModalCallbackFunction::create(
                              [safeThis, playlistId, trackId](int res) {
                                if (res != 0 && safeThis != nullptr) {
                                  safeThis->database.addTrackToPlaylist(
                                      playlistId, trackId);
                                  safeThis->loadPlaylist(playlistId);
                                }
                              }));
                    });
                  } else {
                    db.addTrackToPlaylist(playlistId, trackId);
                  }
                }
              }
            }
          }
        };

    processFiles(paths);
    db.commitTransaction();

    juce::MessageManager::callAsync([safeThis, playlistId]() {
      if (safeThis != nullptr && safeThis->activePlaylistId == playlistId) {
        safeThis->loadPlaylist(playlistId);
      }
    });
  });
}

void TrackBrowserComponent::importFiles(const juce::StringArray &paths) {
  for (const auto &path : paths) {
    juce::File f(path);

    auto processFile = [&](const juce::File &sf) {
      analysisManager.queueTrack(sf);

      // Add to view instantly for real-time update
      bool exists = false;
      for (const auto &t : tracks) {
        if (t.path == sf.getFullPathName()) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        TrackDatabase::Track temp;
        temp.path = sf.getFullPathName();
        temp.name = sf.getFileNameWithoutExtension();
        tracks.push_back(temp);
      }
    };

    if (f.isDirectory()) {
      juce::Array<juce::File> subFiles;
      f.findChildFiles(subFiles, juce::File::findFiles, true,
                       "*.mp3;*.wav;*.flac;*.m4a");
      for (const auto &sf : subFiles)
        processFile(sf);
    } else if (f.existsAsFile()) {
      processFile(f);
    }
  }
  refresh();
}

void TrackBrowserComponent::drawRating(juce::Graphics &g, int rating, int x,
                                       int y, int w, int h) {
  // Usar 20 pixels por bolinha para ficar bem espaçado e fácil de clicar
  for (int i = 0; i < 5; ++i) {
    g.setColour(i < rating ? juce::Colours::cyan
                           : juce::Colours::grey.withAlpha(0.2f));
    float circleX = (float)x + i * 20.0f;
    float circleY = (float)y + (h - 10) / 2.0f;
    g.fillEllipse(circleX, circleY, 10, 10);
  }
}

// TreeView Item Implementation
void TrackBrowserComponent::SidebarItem::paintItem(juce::Graphics &g, int width,
                                                   int height) {
  if (isSelected()) {
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillAll();
    g.setColour(juce::Colours::cyan);
    g.fillRect(0, 0, 3, height);
  }
  g.setColour(isSelected() ? juce::Colours::cyan : juce::Colours::grey);
  g.setFont(
      juce::Font(13.0f, isSelected() ? juce::Font::bold : juce::Font::plain));
  g.drawText(name, 10, 0, width - 20, height, juce::Justification::centredLeft);
}

juce::var TrackBrowserComponent::SidebarItem::getDragSourceDescription() {
  return name;
}

void TrackBrowserComponent::SidebarItem::itemClicked(
    const juce::MouseEvent &e) {
  if (name == "COLLECTION")
    owner.loadCollection();
  else if (name == "RECORDINGS") {
    auto musicDir =
        juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    auto tridjsDir = musicDir.getChildFile("tridjs_lifeStudio");
    owner.loadFolder(tridjsDir,
                     true); // Search recursively for recordings/samplers
  }
}

bool TrackBrowserComponent::SidebarItem::isInterestedInDragSource(
    const juce::DragAndDropTarget::SourceDetails &) {
  return false;
}
void TrackBrowserComponent::SidebarItem::itemDropped(
    const juce::DragAndDropTarget::SourceDetails &, int) {}

void TrackBrowserComponent::PlaylistRootItem::itemOpennessChanged(
    bool isNowOpen) {
  if (isNowOpen) {
    clearSubItems();
    std::vector<TrackDatabase::Playlist> playlists;
    owner.database.getAllPlaylists(playlists);
    for (const auto &p : playlists)
      addSubItem(new PlaylistItem(owner, p.id, p.name));
  }
}

void TrackBrowserComponent::PlaylistRootItem::itemClicked(
    const juce::MouseEvent &e) {
  SidebarItem::itemClicked(e);
  setOpen(true);
  if (e.mods.isRightButtonDown()) {
    juce::PopupMenu m;
    m.addItem(1, "New Playlist...");
    m.showMenuAsync(juce::PopupMenu::Options(), [this](int res) {
      if (res == 1) {
        juce::String defaultName =
            "New Playlist " + juce::String(juce::Random().nextInt(100));
        if (owner.database.addPlaylist(defaultName) <= 0) {
          owner.database.addPlaylist(
              "New Playlist " + juce::String(juce::Random().nextInt(10000)));
        }
        itemOpennessChanged(true);
        owner.repaint();
      }
    });
  }
}

bool TrackBrowserComponent::PlaylistRootItem::isInterestedInDragSource(
    const juce::DragAndDropTarget::SourceDetails &) {
  return true;
}

void TrackBrowserComponent::PlaylistRootItem::itemDropped(
    const juce::DragAndDropTarget::SourceDetails &d, int) {
  juce::StringArray paths;
  paths.addTokens(d.description.toString(), "|", "");

  for (const auto &path : paths) {
    juce::File f(path);
    if (f.isDirectory()) {
      // Create a new playlist with the folder name
      juce::String folderName = f.getFileName();
      int pId = owner.database.addPlaylist(folderName);
      if (pId <= 0) {
        // Find existing
        std::vector<TrackDatabase::Playlist> playlists;
        owner.database.getAllPlaylists(playlists);
        for (const auto &p : playlists)
          if (p.name == folderName) {
            pId = p.id;
            break;
          }
      }

      if (pId > 0) {
        owner.addFilesToPlaylist({path}, pId);
        itemOpennessChanged(true);
      }
    }
  }
}

void TrackBrowserComponent::PlaylistItem::paintItem(juce::Graphics &g,
                                                    int width, int height) {
  SidebarItem::paintItem(g, width, height);
}

void TrackBrowserComponent::PlaylistItem::itemClicked(
    const juce::MouseEvent &e) {
  owner.loadPlaylist(playlistId);

  if (e.mods.isRightButtonDown()) {
    juce::PopupMenu m;
    m.addItem(1, "Delete Playlist...");
    m.showMenuAsync(juce::PopupMenu::Options(), [this](int res) {
      if (res == 1) {
        auto &ownerRef = owner;
        auto pId = playlistId;
        auto pName = name;
        auto *parent = getParentItem();

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon, "Delete Playlist",
            "Are you sure you want to delete '" + pName + "'?", "Yes", "No",
            nullptr,
            juce::ModalCallbackFunction::create(
                [&ownerRef, pId, parent](int result) {
                  if (result != 0) {
                    ownerRef.database.removePlaylist(pId);
                    if (parent != nullptr) {
                      parent->itemOpennessChanged(true);
                    }
                    ownerRef.loadCollection();
                  }
                }));
      }
    });
  }
}

void TrackBrowserComponent::PlaylistItem::itemDoubleClicked(
    const juce::MouseEvent &e) {
  auto *aw = new juce::AlertWindow(
      "Rename Playlist", "Enter new name:", juce::AlertWindow::NoIcon);
  aw->addTextEditor("name", name, "");
  aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
  aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

  auto &ownerRef = owner;
  auto pId = playlistId;
  auto *parent = getParentItem();

  aw->enterModalState(
      true,
      juce::ModalCallbackFunction::create(
          [&ownerRef, pId, parent, aw](int result) {
            if (result == 1) {
              juce::String newName = aw->getTextEditorContents("name");
              if (newName.isNotEmpty()) {
                if (!ownerRef.database.renamePlaylist(pId, newName)) {
                  juce::AlertWindow::showMessageBoxAsync(
                      juce::AlertWindow::WarningIcon, "Erro ao renomear",
                      "Não é possível criar: este nome já está em uso");
                } else if (parent != nullptr) {
                  parent->itemOpennessChanged(true);
                }
              }
            }
          }),
      true);
}

bool TrackBrowserComponent::PlaylistItem::isInterestedInDragSource(
    const juce::DragAndDropTarget::SourceDetails &) {
  return true;
}

void TrackBrowserComponent::PlaylistItem::itemDropped(
    const juce::DragAndDropTarget::SourceDetails &d, int) {
  juce::StringArray paths;
  paths.addTokens(d.description.toString(), "|", "");
  owner.addFilesToPlaylist(paths, playlistId);
}

juce::var TrackBrowserComponent::FileItem::getDragSourceDescription() {
  return file.getFullPathName();
}

void TrackBrowserComponent::ExplorerRootItem::itemOpennessChanged(
    bool isNowOpen) {
  if (isNowOpen) {
    clearSubItems();
    juce::Array<juce::File> roots;
    juce::File::findFileSystemRoots(roots);
    for (const auto &r : roots)
      addSubItem(new FileItem(owner, r));
  }
}

void TrackBrowserComponent::FileItem::itemOpennessChanged(bool isNowOpen) {
  if (isNowOpen && file.isDirectory()) {
    clearSubItems();
    juce::Array<juce::File> children;
    file.findChildFiles(children, juce::File::findFilesAndDirectories, false);
    for (const auto &c : children) {
      if (c.isDirectory() || c.getFileExtension() == ".mp3" ||
          c.getFileExtension() == ".wav")
        addSubItem(new FileItem(owner, c));
    }
  }
}

void TrackBrowserComponent::FileItem::itemClicked(const juce::MouseEvent &e) {
  if (file.isDirectory())
    owner.loadFolder(file);
  else if (owner.onTrackDoubleClicked)
    owner.onTrackDoubleClicked(file);
}
