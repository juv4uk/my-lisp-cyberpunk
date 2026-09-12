#pragma once

namespace player_handle_epoch
{
enum class Action
{
    Keep,
    BindNew,
    BindNil,
    Preserve,
};

// `GetPlayer` може не виконатися, коли RTTI ще недоступний. У такому разі
// зберігаємо чинний handle: відсутність відповіді не є фактом відсутності гравця.
[[nodiscard]] constexpr Action Decide(bool lookupExecuted, const void* observedInstance,
                                      const void* retainedInstance) noexcept
{
    if (!lookupExecuted)
    {
        return Action::Preserve;
    }
    if (observedInstance == nullptr)
    {
        return retainedInstance == nullptr ? Action::Keep : Action::BindNil;
    }
    return observedInstance == retainedInstance ? Action::Keep : Action::BindNew;
}
} // namespace player_handle_epoch