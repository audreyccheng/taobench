/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <utility>
#include <type_traits>

#include <superior_mysqlpp/field_types.hpp>
#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/low_level/dbdriver.hpp>


namespace SuperiorMySqlpp
{
    struct ResultField : private MYSQL_FIELD
    {
    public:
        std::string getColumnName() const
        {
            return {name, name_length};
        }

        const StringView getColumnNameView() const
        {
            return {name, name_length};
        }

        StringView getColumnNameView()
        {
            return {name, name_length};
        }

        std::string getColumnOriginalName() const
        {
            return {org_name, org_name_length};
        }

        const StringView getColumnOriginalNameView() const
        {
            return {org_name, org_name_length};
        }

        StringView getColumnOriginalNameView()
        {
            return {org_name, org_name_length};
        }

        std::string getTableName() const
        {
            return {table, table_length};
        }

        const StringView getTableNameView() const
        {
            return {table, table_length};
        }

        StringView getTableNameView()
        {
            return {table, table_length};
        }

        std::string getTableOriginalName() const
        {
            return {org_table, org_table_length};
        }

        const StringView getTableOriginalNameView() const
        {
            return {org_table, org_table_length};
        }

        StringView getTableOriginalNameView()
        {
            return {org_table, org_table_length};
        }

        std::string getDatabaseName() const
        {
            return {db, db_length};
        }

        const StringView getDatabaseNameView() const
        {
            return {db, db_length};
        }

        StringView getDatabaseNameView()
        {
            return {db, db_length};
        }

        std::string getCatalogName() const
        {
            return {catalog, catalog_length};
        }

        const StringView getCatalogNameView() const
        {
            return {catalog, catalog_length};
        }

        StringView getCatalogNameView()
        {
            return {catalog, catalog_length};
        }

        auto getDisplayLength() const
        {
            return length;
        }

        auto getMaxLength() const
        {
            return max_length;
        }

        bool isNullable() const
        {
            return !(flags & NOT_NULL_FLAG);
        }

        bool isPrimaryKeyPart() const
        {
            return flags & PRI_KEY_FLAG;
        }

        bool isUniqueKeyPart() const
        {
            return flags & UNIQUE_KEY_FLAG;
        }

        bool isNonUniqueKeyPart() const
        {
            return flags & MULTIPLE_KEY_FLAG;
        }

        bool isUnsigned() const
        {
            return flags & UNSIGNED_FLAG;
        }

        bool hasZerofill() const
        {
            return flags & ZEROFILL_FLAG;
        }

        bool isBinary() const
        {
            return flags & BINARY_FLAG;
        }

        bool hasAutoincrement() const
        {
            return flags & AUTO_INCREMENT_FLAG;
        }

        bool isEnum() const
        {
            return flags & ENUM_FLAG;
        }

        bool isSet() const
        {
            return flags & SET_FLAG;
        }

        bool isNumeric() const
        {
            return flags & NUM_FLAG;
        }

        bool hasDefaultValue() const
        {
            return !(flags & NO_DEFAULT_VALUE_FLAG);
        }

        auto getDecimalsCount() const
        {
            return decimals;
        }

        auto getCharacterSetNumber() const
        {
            return charsetnr;
        }

        auto getFieldType() const
        {
            return static_cast<FieldTypes>(type);
        }
    };
    static_assert(
            sizeof(ResultField) == sizeof(MYSQL_FIELD),
            "Internal error. QueryResultField has different layout than MYSQL_FIELD!"
    );


    class ResultMetadata
    {
    public:
        using ValueType = ResultField;
        using Pointer = ValueType*;
        using ConstPointer = const ValueType*;
        using Iterator = ValueType*;
        using ConstIterator = const ValueType*;
        using Reference = ValueType&;
        using ConstReference = const ValueType&;
        using SizeType = unsigned int;

    protected:
        LowLevel::DBDriver::Result result;
        ValueType* memoryStart;
        ValueType* memoryEnd;

    public:
        ResultMetadata(LowLevel::DBDriver::Result&& result)
            : result{std::move(result)},
              memoryStart{reinterpret_cast<ValueType*>(this->result.fetchFields())},
              memoryEnd{reinterpret_cast<ValueType*>(memoryStart + this->result.getFieldsCount())}
        {
            // make sure reinterpret casts above are fine
            static_assert(sizeof(ValueType) == sizeof(decltype(*result.fetchFields())), "ResultMetadata has invalid ValueType!");
        }

        ResultMetadata(ResultMetadata&& resultMetadata) = default;

        ResultMetadata(const ResultMetadata&) = delete;
        ResultMetadata& operator=(const ResultMetadata&) = delete;
        ResultMetadata& operator=(ResultMetadata&&) = delete;

        // Iterators
        Iterator begin() noexcept
        {
            return memoryStart;
        }

        ConstIterator begin() const noexcept
        {
            return memoryStart;
        }

        ConstIterator cbegin() const noexcept
        {
            return memoryStart;
        }

        Iterator end() noexcept
        {
            return memoryEnd;
        }

        ConstIterator end() const noexcept
        {
            return memoryEnd;
        }

        ConstIterator cend() const noexcept
        {
            return memoryEnd;

        }

        // Capacity
        SizeType size() const noexcept
        {
            return memoryEnd - memoryStart;
        }

        SizeType maxSize() const noexcept
        {
            return size();
        }

        bool empty() const noexcept
        {
            return memoryStart == memoryEnd;
        }

        // Elements access
        Reference operator[](SizeType index)
        {
            return *Iterator(memoryStart + index);
        }

        ConstReference operator[](SizeType index) const
        {
            return *ConstIterator(memoryStart + index);
        }

        Reference front()
        {
            return *begin();
        }

        ConstReference front() const
        {
            return *begin();
        }

        Reference back()
        {
            return *(end() - 1);
        }

        ConstReference back() const
        {
            return *(end() - 1);
        }

        Pointer data() noexcept
        {
            return memoryStart;
        }

        ConstPointer data() const noexcept
        {
            return memoryStart;
        }


        /*
         * DO NOT USE this function unless you want to work with C API directly!
         */
        LowLevel::DBDriver::Result& detail_getResult() && = delete;
        LowLevel::DBDriver::Result& detail_getResult() const && = delete;

        LowLevel::DBDriver::Result& detail_getResult() &
        {
            return result;
        }

        const LowLevel::DBDriver::Result& detail_getResult() const &
        {
            return result;
        }
    };
}
