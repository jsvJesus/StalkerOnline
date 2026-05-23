using MySqlConnector;
using StalkerOnline.Server.Database;

namespace StalkerOnline.Server.Services;

public sealed class InventoryRepository
{
    private readonly DatabaseConnectionFactory _connectionFactory;

    public InventoryRepository(DatabaseConnectionFactory connectionFactory)
    {
        _connectionFactory = connectionFactory;
    }

    public InventoryModel GetOrCreateInventory(
        int characterId,
        string inventoryType = "Main",
        int width = 10,
        int height = 6,
        float maxWeight = 50f)
    {
        InventoryModel? existing = FindInventory(characterId, inventoryType);

        if (existing != null)
            return existing;

        try
        {
            using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
            connection.Open();

            using MySqlCommand command = connection.CreateCommand();

            command.CommandText =
                """
                INSERT INTO inventories
                (
                    character_id,
                    inventory_type,
                    width,
                    height,
                    max_weight,
                    created_at_utc,
                    updated_at_utc
                )
                VALUES
                (
                    @character_id,
                    @inventory_type,
                    @width,
                    @height,
                    @max_weight,
                    @created_at_utc,
                    @updated_at_utc
                );
                """;

            DateTime now = DateTime.UtcNow;

            command.Parameters.AddWithValue("@character_id", characterId);
            command.Parameters.AddWithValue("@inventory_type", inventoryType);
            command.Parameters.AddWithValue("@width", width);
            command.Parameters.AddWithValue("@height", height);
            command.Parameters.AddWithValue("@max_weight", maxWeight);
            command.Parameters.AddWithValue("@created_at_utc", now);
            command.Parameters.AddWithValue("@updated_at_utc", now);

            command.ExecuteNonQuery();
        }
        catch (MySqlException ex) when (ex.Number == 1062)
        {
            // inventory already created by another request
        }

        InventoryModel? created = FindInventory(characterId, inventoryType);

        if (created == null)
            throw new InvalidOperationException($"Inventory was not created. CharacterId={characterId}, Type={inventoryType}");

        return created;
    }

    public InventoryModel? FindInventory(int characterId, string inventoryType = "Main")
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                inventory_id,
                character_id,
                inventory_type,
                width,
                height,
                max_weight,
                created_at_utc,
                updated_at_utc
            FROM inventories
            WHERE character_id = @character_id
              AND inventory_type = @inventory_type
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@character_id", characterId);
        command.Parameters.AddWithValue("@inventory_type", inventoryType);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadInventory(reader);
    }

    public List<InventoryItemModel> LoadItems(int inventoryId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                ii.inventory_item_id,
                ii.inventory_id,
                ii.item_template_id,
                it.item_key,
                it.display_name,
                it.max_stack,
                it.weight_per_item,
                ii.quantity,
                ii.slot_x,
                ii.slot_y,
                ii.width,
                ii.height,
                ii.durability,
                ii.created_at_utc,
                ii.updated_at_utc
            FROM inventory_items ii
            INNER JOIN item_templates it
                ON it.item_template_id = ii.item_template_id
            WHERE ii.inventory_id = @inventory_id
            ORDER BY ii.slot_y ASC, ii.slot_x ASC, ii.inventory_item_id ASC;
            """;

        command.Parameters.AddWithValue("@inventory_id", inventoryId);

        using MySqlDataReader reader = command.ExecuteReader();

        List<InventoryItemModel> items = new();

        while (reader.Read())
        {
            items.Add(ReadInventoryItem(reader));
        }

        return items;
    }

    public InventoryItemModel AddItem(
        int inventoryId,
        int itemTemplateId,
        int quantity,
        int slotX,
        int slotY,
        int width = 1,
        int height = 1,
        float durability = 100f)
    {
        InventoryItemModel item = new()
        {
            InventoryId = inventoryId,
            ItemTemplateId = itemTemplateId,
            Quantity = quantity,
            SlotX = slotX,
            SlotY = slotY,
            Width = width,
            Height = height,
            Durability = durability
        };

        return SaveItem(item);
    }

    public InventoryItemModel SaveItem(InventoryItemModel item)
    {
        ValidateInventoryItem(item);

        if (item.InventoryItemId <= 0)
            return InsertItem(item);

        UpdateItem(item);

        InventoryItemModel? updated = FindItemById(item.InventoryItemId);

        if (updated == null)
            throw new InvalidOperationException($"Inventory item disappeared after update. Id={item.InventoryItemId}");

        return updated;
    }

    public InventoryItemModel? FindItemById(long inventoryItemId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                ii.inventory_item_id,
                ii.inventory_id,
                ii.item_template_id,
                it.item_key,
                it.display_name,
                it.max_stack,
                it.weight_per_item,
                ii.quantity,
                ii.slot_x,
                ii.slot_y,
                ii.width,
                ii.height,
                ii.durability,
                ii.created_at_utc,
                ii.updated_at_utc
            FROM inventory_items ii
            INNER JOIN item_templates it
                ON it.item_template_id = ii.item_template_id
            WHERE ii.inventory_item_id = @inventory_item_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@inventory_item_id", inventoryItemId);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadInventoryItem(reader);
    }

    public void DeleteItem(long inventoryItemId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            DELETE FROM inventory_items
            WHERE inventory_item_id = @inventory_item_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@inventory_item_id", inventoryItemId);

        command.ExecuteNonQuery();
    }

    public void ClearInventory(int inventoryId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            DELETE FROM inventory_items
            WHERE inventory_id = @inventory_id;
            """;

        command.Parameters.AddWithValue("@inventory_id", inventoryId);

        command.ExecuteNonQuery();
    }

    public float CalculateWeight(int inventoryId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT COALESCE(SUM(ii.quantity * it.weight_per_item), 0)
            FROM inventory_items ii
            INNER JOIN item_templates it
                ON it.item_template_id = ii.item_template_id
            WHERE ii.inventory_id = @inventory_id;
            """;

        command.Parameters.AddWithValue("@inventory_id", inventoryId);

        object? result = command.ExecuteScalar();

        return Convert.ToSingle(result);
    }

    private InventoryItemModel InsertItem(InventoryItemModel item)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            INSERT INTO inventory_items
            (
                inventory_id,
                item_template_id,
                quantity,
                slot_x,
                slot_y,
                width,
                height,
                durability,
                created_at_utc,
                updated_at_utc
            )
            VALUES
            (
                @inventory_id,
                @item_template_id,
                @quantity,
                @slot_x,
                @slot_y,
                @width,
                @height,
                @durability,
                @created_at_utc,
                @updated_at_utc
            );
            """;

        DateTime now = DateTime.UtcNow;

        command.Parameters.AddWithValue("@inventory_id", item.InventoryId);
        command.Parameters.AddWithValue("@item_template_id", item.ItemTemplateId);
        command.Parameters.AddWithValue("@quantity", item.Quantity);
        command.Parameters.AddWithValue("@slot_x", item.SlotX);
        command.Parameters.AddWithValue("@slot_y", item.SlotY);
        command.Parameters.AddWithValue("@width", item.Width);
        command.Parameters.AddWithValue("@height", item.Height);
        command.Parameters.AddWithValue("@durability", item.Durability);
        command.Parameters.AddWithValue("@created_at_utc", now);
        command.Parameters.AddWithValue("@updated_at_utc", now);

        command.ExecuteNonQuery();

        long inventoryItemId = command.LastInsertedId;

        InventoryItemModel? saved = FindItemById(inventoryItemId);

        if (saved == null)
            throw new InvalidOperationException($"Inventory item was not saved. Id={inventoryItemId}");

        return saved;
    }

    private void UpdateItem(InventoryItemModel item)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            UPDATE inventory_items
            SET
                item_template_id = @item_template_id,
                quantity = @quantity,
                slot_x = @slot_x,
                slot_y = @slot_y,
                width = @width,
                height = @height,
                durability = @durability,
                updated_at_utc = @updated_at_utc
            WHERE inventory_item_id = @inventory_item_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@inventory_item_id", item.InventoryItemId);
        command.Parameters.AddWithValue("@item_template_id", item.ItemTemplateId);
        command.Parameters.AddWithValue("@quantity", item.Quantity);
        command.Parameters.AddWithValue("@slot_x", item.SlotX);
        command.Parameters.AddWithValue("@slot_y", item.SlotY);
        command.Parameters.AddWithValue("@width", item.Width);
        command.Parameters.AddWithValue("@height", item.Height);
        command.Parameters.AddWithValue("@durability", item.Durability);
        command.Parameters.AddWithValue("@updated_at_utc", DateTime.UtcNow);

        command.ExecuteNonQuery();
    }

    private static void ValidateInventoryItem(InventoryItemModel item)
    {
        if (item.InventoryId <= 0)
            throw new InvalidOperationException("InventoryId is invalid.");

        if (item.ItemTemplateId <= 0)
            throw new InvalidOperationException("ItemTemplateId is invalid.");

        if (item.Quantity <= 0)
            throw new InvalidOperationException("Quantity must be greater than zero.");

        if (item.SlotX < 0 || item.SlotY < 0)
            throw new InvalidOperationException("Slot position cannot be negative.");

        if (item.Width <= 0 || item.Height <= 0)
            throw new InvalidOperationException("Item size must be greater than zero.");

        if (item.Durability < 0)
            throw new InvalidOperationException("Durability cannot be negative.");
    }

    private static InventoryModel ReadInventory(MySqlDataReader reader)
    {
        return new InventoryModel
        {
            InventoryId = reader.GetInt32(reader.GetOrdinal("inventory_id")),
            CharacterId = reader.GetInt32(reader.GetOrdinal("character_id")),
            InventoryType = reader.GetString(reader.GetOrdinal("inventory_type")),
            Width = reader.GetInt32(reader.GetOrdinal("width")),
            Height = reader.GetInt32(reader.GetOrdinal("height")),
            MaxWeight = reader.GetFloat(reader.GetOrdinal("max_weight")),
            CreatedAtUtc = reader.GetDateTime(reader.GetOrdinal("created_at_utc")),
            UpdatedAtUtc = reader.GetDateTime(reader.GetOrdinal("updated_at_utc"))
        };
    }

    private static InventoryItemModel ReadInventoryItem(MySqlDataReader reader)
    {
        return new InventoryItemModel
        {
            InventoryItemId = reader.GetInt64(reader.GetOrdinal("inventory_item_id")),
            InventoryId = reader.GetInt32(reader.GetOrdinal("inventory_id")),
            ItemTemplateId = reader.GetInt32(reader.GetOrdinal("item_template_id")),
            ItemKey = reader.GetString(reader.GetOrdinal("item_key")),
            DisplayName = reader.GetString(reader.GetOrdinal("display_name")),
            MaxStack = reader.GetInt32(reader.GetOrdinal("max_stack")),
            WeightPerItem = reader.GetFloat(reader.GetOrdinal("weight_per_item")),
            Quantity = reader.GetInt32(reader.GetOrdinal("quantity")),
            SlotX = reader.GetInt32(reader.GetOrdinal("slot_x")),
            SlotY = reader.GetInt32(reader.GetOrdinal("slot_y")),
            Width = reader.GetInt32(reader.GetOrdinal("width")),
            Height = reader.GetInt32(reader.GetOrdinal("height")),
            Durability = reader.GetFloat(reader.GetOrdinal("durability")),
            CreatedAtUtc = reader.GetDateTime(reader.GetOrdinal("created_at_utc")),
            UpdatedAtUtc = reader.GetDateTime(reader.GetOrdinal("updated_at_utc"))
        };
    }
}