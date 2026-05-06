#include "TrackDatabase.h"
#include "sqlite3.h"

TrackDatabase::TrackDatabase() { open(); }

TrackDatabase::~TrackDatabase() { close(); }

juce::File TrackDatabase::getDatabaseFile() {
  auto appDir =
      juce::File::getSpecialLocation(juce::File::currentExecutableFile)
          .getParentDirectory();
  auto dataDir = appDir.getChildFile("data");
  if (!dataDir.exists())
    dataDir.createDirectory();
  return dataDir.getChildFile("tracks.db");
}

bool TrackDatabase::open() {
  auto file = getDatabaseFile();
  int rc = sqlite3_open(file.getFullPathName().toRawUTF8(), &db);
  if (rc != SQLITE_OK)
    return false;

  createTables();

  return true;
}

void TrackDatabase::close() {
  if (db) {
    sqlite3_close(db);
    db = nullptr;
  }
}

void TrackDatabase::createTables() {
  const char *sql_tracks = "CREATE TABLE IF NOT EXISTS tracks ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "path TEXT UNIQUE,"
                           "name TEXT,"
                           "artist TEXT,"
                           "album TEXT,"
                           "bpm REAL,"
                           "key TEXT,"
                           "rating INTEGER,"
                           "comment TEXT,"
                           "last_played INTEGER,"
                           "created_at INTEGER);";

  const char *sql_analysis =
      "CREATE TABLE IF NOT EXISTS analysis_data ("
      "track_id INTEGER PRIMARY KEY,"
      "waveform BLOB,"
      "spectral BLOB,"
      "beatgrid TEXT,"
      "vocal_stem TEXT,"
      "instrumental_stem TEXT,"
      "beat_stem TEXT,"
      "FOREIGN KEY(track_id) REFERENCES tracks(id) ON DELETE CASCADE);";

  const char *sql_playlists = "CREATE TABLE IF NOT EXISTS playlists ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "name TEXT UNIQUE);";

  const char *sql_playlist_tracks =
      "CREATE TABLE IF NOT EXISTS playlist_tracks ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "playlist_id INTEGER,"
      "track_id INTEGER,"
      "FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,"
      "FOREIGN KEY(track_id) REFERENCES tracks(id) ON DELETE CASCADE);";

  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
  sqlite3_exec(db, sql_tracks, nullptr, nullptr, nullptr);
  sqlite3_exec(db, sql_analysis, nullptr, nullptr, nullptr);
  sqlite3_exec(db, sql_playlists, nullptr, nullptr, nullptr);
  sqlite3_exec(db, sql_playlist_tracks, nullptr, nullptr, nullptr);

  // Add columns if they don't exist (for older databases)
  sqlite3_exec(db, "ALTER TABLE analysis_data ADD COLUMN vocal_stem TEXT;",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "ALTER TABLE analysis_data ADD COLUMN instrumental_stem TEXT;",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db, "ALTER TABLE analysis_data ADD COLUMN beat_stem TEXT;",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db, "ALTER TABLE tracks ADD COLUMN comment TEXT;", nullptr,
               nullptr, nullptr);
}

int TrackDatabase::addOrUpdateTrack(const Track &t) {
  juce::ScopedLock sl(lock);

  // Step 1: Insert only if path doesn't exist yet (preserves rating/comment)
  const char *sql_insert =
      "INSERT OR IGNORE INTO tracks (path, name, artist, album, bpm, key, "
      "rating, comment, last_played, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?, 0, '', ?, ?);";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, t.path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t.name.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.artist.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, t.album.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, t.bpm);
    sqlite3_bind_text(stmt, 6, t.key.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, t.lastPlayed.toMilliseconds());
    sqlite3_bind_int64(stmt, 8, t.createdAt.toMilliseconds());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  // Step 2: Update only metadata fields (never overwrite rating/comment)
  const char *sql_update =
      "UPDATE tracks SET name=?, artist=?, album=?, bpm=?, key=?, "
      "last_played=?, created_at=? WHERE path=?;";

  if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, t.name.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t.artist.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.album.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, t.bpm);
    sqlite3_bind_text(stmt, 5, t.key.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, t.lastPlayed.toMilliseconds());
    sqlite3_bind_int64(stmt, 7, t.createdAt.toMilliseconds());
    sqlite3_bind_text(stmt, 8, t.path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  // Return the ID of the track
  const char *sql_id = "SELECT id FROM tracks WHERE path=?;";
  int id = 0;
  if (sqlite3_prepare_v2(db, sql_id, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, t.path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
  }
  return id;
}

bool TrackDatabase::getTrackByPath(const juce::String &path, Track &t) {
  juce::ScopedLock sl(lock);
  // Use explicit column names to avoid index bugs from ALTER TABLE ordering
  const char *sql =
      "SELECT id, path, name, artist, album, bpm, key, rating, comment, "
      "last_played, created_at FROM tracks WHERE path = ?;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);

  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    t.id      = sqlite3_column_int(stmt, 0);
    t.path    = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 1));
    t.name    = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 2));
    t.artist  = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 3));
    t.album   = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 4));
    t.bpm     = sqlite3_column_double(stmt, 5);
    t.key     = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 6));
    t.rating  = sqlite3_column_int(stmt, 7);
    const char *comm = (const char *)sqlite3_column_text(stmt, 8);
    t.comment = comm ? juce::String::fromUTF8(comm) : "";
    t.lastPlayed = juce::Time(sqlite3_column_int64(stmt, 9));
    t.createdAt  = juce::Time(sqlite3_column_int64(stmt, 10));
    found = true;
  }

  sqlite3_finalize(stmt);
  return found;
}

void TrackDatabase::getAllTracks(std::vector<Track> &results) {
  searchTracks("", results);
}

void TrackDatabase::searchTracks(const juce::String &query,
                                 std::vector<Track> &results,
                                 const juce::String &sortColumn,
                                 bool sortAscending) {
  juce::ScopedLock sl(lock);
  // Use explicit column names to avoid index bugs from ALTER TABLE ordering
  juce::String sqlStr =
      "SELECT id, path, name, artist, album, bpm, key, rating, comment, "
      "last_played, created_at FROM tracks";
  if (query.isNotEmpty()) {
    sqlStr += " WHERE name LIKE ? OR artist LIKE ?";
  }

  // Map column names to DB columns
  juce::String dbCol = sortColumn;
  if (sortColumn == "TRACK NAME")
    dbCol = "name";
  else if (sortColumn == "ARTIST")
    dbCol = "artist";
  else if (sortColumn == "BPM")
    dbCol = "bpm";
  else if (sortColumn == "KEY")
    dbCol = "key";
  else if (sortColumn == "RATING")
    dbCol = "rating";
  else if (sortColumn == "COMMENT")
    dbCol = "comment";

  sqlStr += " ORDER BY " + dbCol + (sortAscending ? " ASC" : " DESC") + ";";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sqlStr.toRawUTF8(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    return;

  if (query.isNotEmpty()) {
    juce::String wildQuery = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, wildQuery.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, wildQuery.toRawUTF8(), -1, SQLITE_TRANSIENT);
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Track t;
    t.id = sqlite3_column_int(stmt, 0);
    t.path = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 1));
    t.name = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 2));
    t.artist =
        juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 3));
    t.album =
        juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 4));
    t.bpm = sqlite3_column_double(stmt, 5);
    t.key = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 6));
    t.rating = sqlite3_column_int(stmt, 7);
    const char *comm = (const char *)sqlite3_column_text(stmt, 8);
    t.comment = comm ? juce::String::fromUTF8(comm) : "";
    t.lastPlayed = juce::Time(sqlite3_column_int64(stmt, 9));
    t.createdAt = juce::Time(sqlite3_column_int64(stmt, 10));
    results.push_back(t);
  }

  sqlite3_finalize(stmt);
}

void TrackDatabase::updateRating(int trackId, int rating) {
  juce::ScopedLock sl(lock);
  juce::String sql = "UPDATE tracks SET rating = " + juce::String(rating) +
                     " WHERE id = " + juce::String(trackId) + ";";
  sqlite3_exec(db, sql.toRawUTF8(), nullptr, nullptr, nullptr);
}

void TrackDatabase::updateComment(int trackId, const juce::String &comment) {
  juce::ScopedLock sl(lock);
  const char *sql = "UPDATE tracks SET comment = ? WHERE id = ?;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, comment.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, trackId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

void TrackDatabase::saveAnalysis(const Analysis &a) {
  juce::ScopedLock sl(lock);
  const char *sql = "INSERT OR REPLACE INTO analysis_data (track_id, waveform, "
                    "spectral, beatgrid, vocal_stem, instrumental_stem, "
                    "beat_stem) VALUES (?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return;

  sqlite3_bind_int(stmt, 1, a.trackId);
  sqlite3_bind_blob(stmt, 2, a.waveform.getData(), (int)a.waveform.getSize(),
                    SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 3, a.spectral.getData(), (int)a.spectral.getSize(),
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, a.beatgrid.toRawUTF8(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, a.vocalStemPath.toRawUTF8(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, a.instrumentalStemPath.toRawUTF8(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, a.beatStemPath.toRawUTF8(), -1, SQLITE_TRANSIENT);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

bool TrackDatabase::loadAnalysis(int trackId, Analysis &a) {
  juce::ScopedLock sl(lock);
  const char *sql =
      "SELECT track_id, waveform, spectral, beatgrid, vocal_stem, "
      "instrumental_stem, beat_stem FROM analysis_data WHERE track_id = ?;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int(stmt, 1, trackId);

  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    a.trackId = trackId;
    const void *wavData = sqlite3_column_blob(stmt, 1);
    int wavSize = sqlite3_column_bytes(stmt, 1);
    a.waveform.replaceWith(wavData, wavSize);

    const void *specData = sqlite3_column_blob(stmt, 2);
    int specSize = sqlite3_column_bytes(stmt, 2);
    a.spectral.replaceWith(specData, specSize);

    const char *beatgridText = (const char *)sqlite3_column_text(stmt, 3);
    a.beatgrid =
        beatgridText ? juce::String::fromUTF8(beatgridText) : juce::String();

    const char *vocalText = (const char *)sqlite3_column_text(stmt, 4);
    a.vocalStemPath =
        vocalText ? juce::String::fromUTF8(vocalText) : juce::String();

    const char *instText = (const char *)sqlite3_column_text(stmt, 5);
    a.instrumentalStemPath =
        instText ? juce::String::fromUTF8(instText) : juce::String();

    const char *beatText = (const char *)sqlite3_column_text(stmt, 6);
    a.beatStemPath =
        beatText ? juce::String::fromUTF8(beatText) : juce::String();

    found = true;
  }

  sqlite3_finalize(stmt);
  return found;
}

void TrackDatabase::beginTransaction() {
  juce::ScopedLock sl(lock);
  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
}

void TrackDatabase::commitTransaction() {
  juce::ScopedLock sl(lock);
  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

void TrackDatabase::updateLastPlayed(int trackId) {
  juce::ScopedLock sl(lock);
  juce::String sql =
      "UPDATE tracks SET last_played = " +
      juce::String(juce::Time::getCurrentTime().toMilliseconds()) +
      " WHERE id = " + juce::String(trackId) + ";";
  sqlite3_exec(db, sql.toRawUTF8(), nullptr, nullptr, nullptr);
}

void TrackDatabase::enforceCacheLimit(juce::int64 maxSizeBytes) {
  auto file = getDatabaseFile();
  if (file.getSize() < maxSizeBytes)
    return;

  juce::ScopedLock sl(lock);
  sqlite3_exec(db,
               "DELETE FROM analysis_data WHERE track_id IN (SELECT id FROM "
               "tracks ORDER BY last_played ASC LIMIT 10);",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db, "VACUUM;", nullptr, nullptr, nullptr);
}

int TrackDatabase::addPlaylist(const juce::String &name) {
  if (isPlaylistNameTaken(name))
    return -1;

  juce::ScopedLock sl(lock);
  const char *sql = "INSERT INTO playlists (name) VALUES (?);";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(stmt, 1, name.toRawUTF8(), -1, SQLITE_TRANSIENT);

  int result = sqlite3_step(stmt);
  int id = 0;
  if (result == SQLITE_DONE)
    id = (int)sqlite3_last_insert_rowid(db);
  else if (result == SQLITE_CONSTRAINT)
    id = -1;

  sqlite3_finalize(stmt);
  return id;
}

void TrackDatabase::removePlaylist(int playlistId) {
  juce::ScopedLock sl(lock);
  juce::String sql =
      "DELETE FROM playlists WHERE id = " + juce::String(playlistId) + ";";
  sqlite3_exec(db, sql.toRawUTF8(), nullptr, nullptr, nullptr);
}

bool TrackDatabase::renamePlaylist(int playlistId,
                                   const juce::String &newName) {
  if (isPlaylistNameTaken(newName))
    return false;

  juce::ScopedLock sl(lock);
  const char *sql = "UPDATE playlists SET name = ? WHERE id = ?;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_text(stmt, 1, newName.toRawUTF8(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, playlistId);

  int result = sqlite3_step(stmt);
  bool success = (result == SQLITE_DONE);

  sqlite3_finalize(stmt);
  return success;
}

bool TrackDatabase::isPlaylistNameTaken(const juce::String &name) {
  juce::ScopedLock sl(lock);
  const char *sql = "SELECT 1 FROM playlists WHERE name = ? LIMIT 1;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_text(stmt, 1, name.toRawUTF8(), -1, SQLITE_TRANSIENT);
  bool taken = (sqlite3_step(stmt) == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return taken;
}

void TrackDatabase::getAllPlaylists(std::vector<Playlist> &results) {
  juce::ScopedLock sl(lock);
  const char *sql = "SELECT * FROM playlists ORDER BY name ASC;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    results.push_back(
        {sqlite3_column_int(stmt, 0),
         juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 1))});
  }
  sqlite3_finalize(stmt);
}

void TrackDatabase::addTrackToPlaylist(int playlistId, int trackId) {
  juce::ScopedLock sl(lock);
  const char *sql =
      "INSERT INTO playlist_tracks (playlist_id, track_id) VALUES (?, ?);";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return;
  sqlite3_bind_int(stmt, 1, playlistId);
  sqlite3_bind_int(stmt, 2, trackId);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

bool TrackDatabase::isTrackInPlaylist(int playlistId, int trackId) {
  juce::ScopedLock sl(lock);
  const char *sql = "SELECT 1 FROM playlist_tracks WHERE playlist_id = ? AND "
                    "track_id = ? LIMIT 1;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int(stmt, 1, playlistId);
  sqlite3_bind_int(stmt, 2, trackId);
  bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return exists;
}

void TrackDatabase::removeTrackFromPlaylist(int playlistId, int trackId) {
  juce::ScopedLock sl(lock);
  juce::String sql = "DELETE FROM playlist_tracks WHERE playlist_id = " +
                     juce::String(playlistId) +
                     " AND track_id = " + juce::String(trackId) + ";";
  sqlite3_exec(db, sql.toRawUTF8(), nullptr, nullptr, nullptr);
}

void TrackDatabase::removeTrackEntry(int entryId) {
  juce::ScopedLock sl(lock);
  juce::String sql =
      "DELETE FROM playlist_tracks WHERE id = " + juce::String(entryId) + ";";
  sqlite3_exec(db, sql.toRawUTF8(), nullptr, nullptr, nullptr);
}

void TrackDatabase::removeTrackFromCollection(int trackId) {
  juce::ScopedLock sl(lock);
  juce::String sql =
      "DELETE FROM tracks WHERE id = " + juce::String(trackId) + ";";
  sqlite3_exec(db, sql.toRawUTF8(), nullptr, nullptr, nullptr);
}

void TrackDatabase::getTracksInPlaylist(int playlistId,
                                        std::vector<Track> &results) {
  juce::ScopedLock sl(lock);
  // Use explicit column names to avoid index bugs from ALTER TABLE ordering
  juce::String sqlStr =
      "SELECT t.id, t.path, t.name, t.artist, t.album, t.bpm, t.key, "
      "t.rating, t.comment, t.last_played, t.created_at, pt.id as entry_id "
      "FROM tracks t "
      "JOIN playlist_tracks pt ON t.id = pt.track_id "
      "WHERE pt.playlist_id = " +
      juce::String(playlistId) + " ORDER BY pt.id ASC;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sqlStr.toRawUTF8(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    return;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Track t;
    t.id      = sqlite3_column_int(stmt, 0);
    t.path    = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 1));
    t.name    = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 2));
    t.artist  = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 3));
    t.album   = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 4));
    t.bpm     = sqlite3_column_double(stmt, 5);
    t.key     = juce::String::fromUTF8((const char *)sqlite3_column_text(stmt, 6));
    t.rating  = sqlite3_column_int(stmt, 7);
    const char *comm = (const char *)sqlite3_column_text(stmt, 8);
    t.comment = comm ? juce::String::fromUTF8(comm) : "";
    t.lastPlayed      = juce::Time(sqlite3_column_int64(stmt, 9));
    t.createdAt       = juce::Time(sqlite3_column_int64(stmt, 10));
    t.playlistEntryId = sqlite3_column_int(stmt, 11);
    results.push_back(t);
  }
  sqlite3_finalize(stmt);
}
