#pragma once
#include <JuceHeader.h>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

class TrackDatabase
{
public:
    struct Track {
        int id = 0;
        juce::String path;
        juce::String name;
        juce::String artist;
        juce::String album;
        double bpm = 0.0;
        juce::String key;
        int rating = 0;
        juce::String comment;
        juce::Time lastPlayed;
        juce::Time createdAt;
        int playlistEntryId = 0; // ID from playlist_tracks table
    };

    struct Analysis {
        int trackId = 0;
        juce::MemoryBlock waveform;
        juce::MemoryBlock spectral;
        juce::String beatgrid;
        juce::String vocalStemPath;
        juce::String instrumentalStemPath;
        juce::String beatStemPath;
    };

    TrackDatabase();
    ~TrackDatabase();

    bool open();
    void close();

    // CRUD
    int addOrUpdateTrack(const Track& track);
    bool getTrackByPath(const juce::String& path, Track& track);
    void getAllTracks(std::vector<Track>& results);
    void searchTracks(const juce::String& query, std::vector<Track>& results, const juce::String& sortColumn = "name", bool sortAscending = true);
    
    // Partial Updates
    void updateRating(int trackId, int rating);
    void updateComment(int trackId, const juce::String& comment);
    
    
    // Analysis
    void saveAnalysis(const Analysis& analysis);
    bool loadAnalysis(int trackId, Analysis& analysis);

    // Playlists
    struct Playlist {
        int id = 0;
        juce::String name;
    };
    int addPlaylist(const juce::String& name);
    void removePlaylist(int playlistId);
    bool renamePlaylist(int playlistId, const juce::String& newName);
    bool isPlaylistNameTaken(const juce::String& name);
    void getAllPlaylists(std::vector<Playlist>& results);
    void addTrackToPlaylist(int playlistId, int trackId);
    void removeTrackFromPlaylist(int playlistId, int trackId); // Removes all instances
    void removeTrackEntry(int entryId); // Removes specific instance
    void removeTrackFromCollection(int trackId); // Removes from database permanently
    bool isTrackInPlaylist(int playlistId, int trackId);
    void getTracksInPlaylist(int playlistId, std::vector<Track>& results);

    // Transactions
    void beginTransaction();
    void commitTransaction();
    
    // Cache management
    void updateLastPlayed(int trackId);
    void enforceCacheLimit(juce::int64 maxSizeBytes = 5LL * 1024 * 1024 * 1024); // 5GB

private:
    sqlite3* db = nullptr;
    juce::CriticalSection lock;
    
    void createTables();
    juce::File getDatabaseFile();
};
