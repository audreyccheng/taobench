/*
 * Author: Tomas Nozicka
 */

#pragma once


namespace SuperiorMySqlpp
{
    /**
     * Tag used to specify overload in resolution.
     * As name suggests, it should denote "full, explicit initialization".
     * @remark Consider similarities with std::piecewise_construct.
     */
    struct FullInitTag {};
    constexpr FullInitTag fullInitTag{};
}
