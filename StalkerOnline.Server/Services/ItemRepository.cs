using MySqlConnector;
using StalkerOnline.Server.Database;

namespace StalkerOnline.Server.Services;

public sealed class ItemRepository
{
    private readonly DatabaseConnectionFactory _connectionFactory;

    public ItemRepository(DatabaseConnectionFactory connectionFactory)
    {
        _connectionFactory = connectionFactory;
    }

    public ItemTemplateModel? FindById(int itemTemplateId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                item_template_id,
                item_key,
                display_name,
                item_type,
                rarity,
                max_stack,
                weight_per_item,
                max_durability,
                food_value,
                water_value,
                radiation_protection,
                created_at_utc,
                updated_at_utc
            FROM item_templates
            WHERE item_template_id = @item_template_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@item_template_id", itemTemplateId);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadItemTemplate(reader);
    }

    public ItemTemplateModel? FindByKey(string itemKey)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                item_template_id,
                item_key,
                display_name,
                item_type,
                rarity,
                max_stack,
                weight_per_item,
                max_durability,
                food_value,
                water_value,
                radiation_protection,
                created_at_utc,
                updated_at_utc
            FROM item_templates
            WHERE item_key = @item_key
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@item_key", itemKey);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadItemTemplate(reader);
    }

    public List<ItemTemplateModel> GetAll()
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                item_template_id,
                item_key,
                display_name,
                item_type,
                rarity,
                max_stack,
                weight_per_item,
                max_durability,
                food_value,
                water_value,
                radiation_protection,
                created_at_utc,
                updated_at_utc
            FROM item_templates
            ORDER BY item_template_id ASC;
            """;

        using MySqlDataReader reader = command.ExecuteReader();

        List<ItemTemplateModel> items = new();

        while (reader.Read())
        {
            items.Add(ReadItemTemplate(reader));
        }

        return items;
    }

    public ItemTemplateModel CreateOrUpdate(ItemTemplateModel item)
    {
        ValidateItemTemplate(item);

        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            INSERT INTO item_templates
            (
                item_key,
                display_name,
                item_type,
                rarity,
                max_stack,
                weight_per_item,
                max_durability,
                food_value,
                water_value,
                radiation_protection,
                created_at_utc,
                updated_at_utc
            )
            VALUES
            (
                @item_key,
                @display_name,
                @item_type,
                @rarity,
                @max_stack,
                @weight_per_item,
                @max_durability,
                @food_value,
                @water_value,
                @radiation_protection,
                @created_at_utc,
                @updated_at_utc
            )
            ON DUPLICATE KEY UPDATE
                display_name = VALUES(display_name),
                item_type = VALUES(item_type),
                rarity = VALUES(rarity),
                max_stack = VALUES(max_stack),
                weight_per_item = VALUES(weight_per_item),
                max_durability = VALUES(max_durability),
                food_value = VALUES(food_value),
                water_value = VALUES(water_value),
                radiation_protection = VALUES(radiation_protection),
                updated_at_utc = VALUES(updated_at_utc);
            """;

        DateTime now = DateTime.UtcNow;

        command.Parameters.AddWithValue("@item_key", item.ItemKey);
        command.Parameters.AddWithValue("@display_name", item.DisplayName);
        command.Parameters.AddWithValue("@item_type", item.ItemType);
        command.Parameters.AddWithValue("@rarity", item.Rarity);
        command.Parameters.AddWithValue("@max_stack", item.MaxStack);
        command.Parameters.AddWithValue("@weight_per_item", item.WeightPerItem);
        command.Parameters.AddWithValue("@max_durability", item.MaxDurability);
        command.Parameters.AddWithValue("@food_value", item.FoodValue);
        command.Parameters.AddWithValue("@water_value", item.WaterValue);
        command.Parameters.AddWithValue("@radiation_protection", item.RadiationProtection);
        command.Parameters.AddWithValue("@created_at_utc", now);
        command.Parameters.AddWithValue("@updated_at_utc", now);

        command.ExecuteNonQuery();

        ItemTemplateModel? savedItem = FindByKey(item.ItemKey);

        if (savedItem == null)
            throw new InvalidOperationException($"Item template was not saved: {item.ItemKey}");

        return savedItem;
    }

    private static void ValidateItemTemplate(ItemTemplateModel item)
    {
        if (string.IsNullOrWhiteSpace(item.ItemKey))
            throw new InvalidOperationException("ItemKey is empty.");

        if (string.IsNullOrWhiteSpace(item.DisplayName))
            throw new InvalidOperationException("DisplayName is empty.");

        if (string.IsNullOrWhiteSpace(item.ItemType))
            throw new InvalidOperationException("ItemType is empty.");

        if (string.IsNullOrWhiteSpace(item.Rarity))
            throw new InvalidOperationException("Rarity is empty.");

        if (item.MaxStack <= 0)
            throw new InvalidOperationException("MaxStack must be greater than zero.");

        if (item.WeightPerItem < 0)
            throw new InvalidOperationException("WeightPerItem cannot be negative.");

        if (item.MaxDurability < 0)
            throw new InvalidOperationException("MaxDurability cannot be negative.");
    }

    private static ItemTemplateModel ReadItemTemplate(MySqlDataReader reader)
    {
        return new ItemTemplateModel
        {
            ItemTemplateId = reader.GetInt32(reader.GetOrdinal("item_template_id")),
            ItemKey = reader.GetString(reader.GetOrdinal("item_key")),
            DisplayName = reader.GetString(reader.GetOrdinal("display_name")),
            ItemType = reader.GetString(reader.GetOrdinal("item_type")),
            Rarity = reader.GetString(reader.GetOrdinal("rarity")),
            MaxStack = reader.GetInt32(reader.GetOrdinal("max_stack")),
            WeightPerItem = reader.GetFloat(reader.GetOrdinal("weight_per_item")),
            MaxDurability = reader.GetFloat(reader.GetOrdinal("max_durability")),
            FoodValue = reader.GetFloat(reader.GetOrdinal("food_value")),
            WaterValue = reader.GetFloat(reader.GetOrdinal("water_value")),
            RadiationProtection = reader.GetFloat(reader.GetOrdinal("radiation_protection")),
            CreatedAtUtc = reader.GetDateTime(reader.GetOrdinal("created_at_utc")),
            UpdatedAtUtc = reader.GetDateTime(reader.GetOrdinal("updated_at_utc"))
        };
    }
}