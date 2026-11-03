#pragma once

#include <string_view>

namespace scwx::qt::render
{

void InitializeGraphics();

/**
 * @brief Checks that the system can run the render backend, and warns the user
 * if it can not. Requires a QApplication. On Vulkan builds, a probe instance is
 * created and at least one physical device must support VK_KHR_swapchain and a
 * graphics queue (Vulkan 1.0 is sufficient). On Metal builds this is a no-op.
 *
 * @return false if the user chose to exit instead of continuing
 */
bool CheckGraphicsSupport();

/**
 * @brief Logs a graphics initialization failure that happened after startup
 * (e.g. QRhi could not be created), and shows the unsupported graphics dialog
 * once per run. May be called from any thread.
 *
 * @param [in] reason Description of the failure for the log
 */
void ReportGraphicsInitFailure(std::string_view reason);

} // namespace scwx::qt::render
