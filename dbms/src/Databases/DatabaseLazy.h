#pragma once

#include <Databases/DatabaseOnDisk.h>
#include <Interpreters/Context.h>
#include <Parsers/ASTCreateQuery.h>


namespace DB
{


class DatabaseLazyIterator;

/** Lazy engine of databases.
  * Works like DatabaseOrdinary, but stores in memory only cache.
  * Can be used only with *Log engines.
  */
class DatabaseLazy : public DatabaseOnDisk
{
public:
    DatabaseLazy(const String & name_, const String & metadata_path_, time_t expiration_time_, const Context & context_);

    String getEngineName() const override { return "Lazy"; }

    void loadStoredObjects(
        Context & context,
        bool has_force_restore_data_flag) override;

    void createTable(
        const Context & context,
        const String & table_name,
        const StoragePtr & table,
        const ASTPtr & query) override;

    void removeTable(
        const Context & context,
        const String & table_name) override;

    void renameTable(
        const Context & context,
        const String & table_name,
        IDatabase & to_database,
        const String & to_table_name,
        TableStructureWriteLockHolder &) override;

    void alterTable(
        const Context & context,
        const String & name,
        const StorageInMemoryMetadata & metadata) override;

    time_t getObjectMetadataModificationTime(const String & table_name) const override;

    bool isTableExist(
        const Context & context,
        const String & table_name) const override;

    StoragePtr tryGetTable(
        const Context & context,
        const String & table_name) const override;

    bool empty(const Context & context) const override;

    DatabaseTablesIteratorPtr getTablesIterator(const Context & context, const FilterByNameFunction & filter_by_table_name) override;

    void attachTable(const String & table_name, const StoragePtr & table) override;

    StoragePtr detachTable(const String & table_name) override;

    void shutdown() override;

    ~DatabaseLazy() override;

private:
    struct CacheExpirationQueueElement
    {
        time_t last_touched;
        String table_name;

        CacheExpirationQueueElement(time_t last_touched_, const String & table_name_)
            : last_touched(last_touched_), table_name(table_name_) {}
    };

    using CacheExpirationQueue = std::list<CacheExpirationQueueElement>;


    struct CachedTable
    {
        StoragePtr table;
        time_t last_touched;
        time_t metadata_modification_time;
        CacheExpirationQueue::iterator expiration_iterator;

        CachedTable() {}
        CachedTable(const StoragePtr & table_, time_t last_touched_, time_t metadata_modification_time_)
            : table(table_), last_touched(last_touched_), metadata_modification_time(metadata_modification_time_) {}
    };

    using TablesCache = std::unordered_map<String, CachedTable>;

    const time_t expiration_time;

    /// TODO use DatabaseWithOwnTablesBase::tables
    mutable TablesCache tables_cache;
    mutable CacheExpirationQueue cache_expiration_queue;

    StoragePtr loadTable(const Context & context, const String & table_name) const;

    void clearExpiredTables() const;

    friend class DatabaseLazyIterator;
};


class DatabaseLazyIterator final : public IDatabaseTablesIterator
{
public:
    DatabaseLazyIterator(
        DatabaseLazy & database_,
        const Context & context_,
        Strings && table_names_);

    void next() override;
    bool isValid() const override;
    const String & name() const override;
    const StoragePtr & table() const override;

private:
    const DatabaseLazy & database;
    const Strings table_names;
    const Context context;
    Strings::const_iterator iterator;
    mutable StoragePtr current_storage;
};
}
