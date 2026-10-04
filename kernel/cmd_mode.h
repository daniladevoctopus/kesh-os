#ifndef KERNEL_CMD_MODE_H
#define KERNEL_CMD_MODE_H

/**
 * @brief Enter the interactive kernel-space command line interface (CMD Mode).
 * 
 * In this mode, KeshOS operates without a graphical desktop compositor, providing
 * direct bare-metal hardware and subsystem control from Ring 0.
 * Typing 'desktop' or 'exit' returns from this function, allowing the system
 * to continue launching the graphical Desktop Shell.
 */
void kernel_cmd_mode(void);

#endif /* KERNEL_CMD_MODE_H */
