/**
 * process.c
 * Handles all command processing and cell operations in the spreadsheet
 */

#include <math.h>
#include <unistd.h>
#include <limits.h>
#include "init.h"
#include "io.h"
#include "process.h"
#include "display.h"
#include "string.h"
#include <stdio.h>
#include "dependent.h"

#define ERROR_VALUE INT_MIN

// void assign(ParsedCommand *result) {
//     int r1 = result->op1.row - 1;
//     int c1 = result->op1.col - 1;

//     int r2 = result->op2.row - 1;
//     int c2 = result->op2.col - 1;
//     int val = result->op2.value;

//     if (r2 == -1 && c2 == -1) {
//         // Remove r1, c1 from child lists of its parents
//         Parent *parent = Parent_lst[r1][c1];
//         while (parent != NULL) {
//             remove_child(parent->r, parent->c, r1, c1);
//             parent = parent->next;
//         }

//         // Remove all parents of r1, c1 using remove_parent
//         while (Parent_lst[r1][c1] != NULL) {
//             remove_parent(Parent_lst[r1][c1]->r, Parent_lst[r1][c1]->c, r1, c1);
//         }

//         sheet[r1][c1] = val;
//     } else {
//         // Assign parent-child relationship
//         assign_parent(r2, c2, r1, c1, *result);
//         assign_child(r2, c2, r1, c1, *result);

//         // Detect cycle
//         bool cycle = detect_cycle(r1, c1);
//         if (cycle) {
//             // Rollback changes if cycle is detected
//             remove_child(r2, c2, r1, c1);
//             remove_parent(r2, c2, r1, c1);
//             printf("Cycle detected! Assignment aborted.\n");
//         } else {
//             // Remove previous dependencies of r1, c1
//             Parent *parent = Parent_lst[r1][c1];
//             while (parent != NULL) {
//                 remove_child(parent->r, parent->c, r1, c1);
//                 parent = parent->next;
//             }

//             while (Parent_lst[r1][c1] != NULL) {
//                 remove_parent(Parent_lst[r1][c1]->r, Parent_lst[r1][c1]->c, r1, c1);
//             }

//             // Reassign parent-child relationship after removing old dependencies
//             assign_parent(r2, c2, r1, c1, *result);
//             assign_child(r2, c2, r1, c1, *result);

//             // Update cell value
//             sheet[r1][c1] = sheet[r2][c2];
//         }
//     }
// }

/**
 * Assigns a value to a cell and handles dependencies
 * - Supports direct value assignment and cell reference assignment
 * - Updates dependent cells, especially those with SLEEP functions
 * - Maintains parent-child relationships in dependency graph
 */
void assign(ParsedCommand *result) {
    int r1 = result->op1.row - 1;
    int c1 = result->op1.col - 1;
    int r2 = result->op2.row - 1;
    int c2 = result->op2.col - 1;
    int val = result->op2.value;

    if (r2 == -1 && c2 == -1) {
        // Remove previous dependencies but store child formulas
        Child *children = Child_lst[r1][c1];
        ParsedCommand *child_formulas = NULL;
        int child_count = 0;
        
        // Count and store child formulas
        Child *curr = children;
        while (curr != NULL) {
            child_count++;
            curr = curr->next;
        }
        
        if (child_count > 0) {
            child_formulas = (ParsedCommand *)malloc(child_count * sizeof(ParsedCommand));
            int i = 0;
            curr = children;
            while (curr != NULL) {
                child_formulas[i] = curr->formula;
                i++;
                curr = curr->next;
            }
        }

        // Remove old dependencies
        Parent *parent = Parent_lst[r1][c1];
        while (parent != NULL) {
            remove_child(parent->r, parent->c, r1, c1);
            parent = parent->next;
        }

        while (Parent_lst[r1][c1] != NULL) {
            remove_parent(Parent_lst[r1][c1]->r, Parent_lst[r1][c1]->c, r1, c1);
        }

        sheet[r1][c1] = val;

        // Restore and update children
        if (child_count > 0) {
            for (int i = 0; i < child_count; i++) {
                ParsedCommand cmd = child_formulas[i];
                int child_r = cmd.op1.row - 1;
                int child_c = cmd.op1.col - 1;
                
                // Re-establish dependencies
                if (cmd.func == FUNC_SLEEP) {
                    int sleep_duration = sheet[r1][c1];
                    if (sleep_duration >= 0 && sleep_duration <= 3600) {
                        sleep(sleep_duration);
                        sheet[child_r][child_c] = sleep_duration;
                    }
                } else {
                    // Re-process the child's formula
                    function(&cmd);
                }
                
                // Restore the dependency
                assign_parent(r1, c1, child_r, child_c, cmd);
                assign_child(r1, c1, child_r, child_c, cmd);
            }
            free(child_formulas);
        }
    } else {
        sheet[r1][c1] = sheet[r2][c2];
    }
}

/**
 * Performs arithmetic operations between cells or values
 * - Supports +, -, *, / operations
 * - Handles cell references and direct values
 * - Sets ERROR_VALUE for division by zero
 */
void arithmetic(ParsedCommand *result) {
    int r1 = result->op1.row - 1;
    int c1 = result->op1.col - 1;
    int r2 = result->op2.row - 1;
    int c2 = result->op2.col - 1;
    int r3 = result->op3.row - 1;
    int c3 = result->op3.col - 1;
    int val2 = result->op2.value;
    int val3 = result->op3.value;

    int operand1 = (r2 == -1 && c2 == -1) ? val2 : sheet[r2][c2];
    int operand2 = (r3 == -1 && c3 == -1) ? val3 : sheet[r3][c3];

    switch (result->operator) {
        case '+':
            sheet[r1][c1] = operand1 + operand2;
            break;
        case '-':
            sheet[r1][c1] = operand1 - operand2;
            break;
        case '*':
            sheet[r1][c1] = operand1 * operand2;
            break;
        case '/':
            if (operand2 == 0) {
                sheet[r1][c1] = ERROR_VALUE;
            } else {
                sheet[r1][c1] = operand1 / operand2;
            }
            break;
    }
    // Handle dependencies
}

/**
 * Validates if a range of cells is within bounds and properly ordered
 * @param r1,c1 Starting cell coordinates (0-based)
 * @param r2,c2 Ending cell coordinates (0-based)
 * @return true if range is valid, false otherwise
 */
bool is_valid_range(ParsedCommand* cmd) {
    // For range operations
    if (cmd->func == FUNC_MIN || cmd->func == FUNC_MAX || cmd->func == FUNC_SUM || 
        cmd->func == FUNC_AVG || cmd->func == FUNC_STDEV) {
        
        int r1 = cmd->op2.row - 1;
        int c1 = cmd->op2.col - 1;
        int r2 = cmd->op3.row - 1;
        int c2 = cmd->op3.col - 1;

        // Check if range is forward-moving
        if (r1 > r2 || c1 > c2) {
            return false;
        }

        // Check if range is within bounds
        if (r1 < 0 || r1 >= MAXROW || c1 < 0 || c1 >= MAXCOL ||
            r2 < 0 || r2 >= MAXROW || c2 < 0 || c2 >= MAXCOL) {
            return false;
        }

        return true;
    }
    return true;  // Non-range operations are always valid
}

/**
 * Main dependency handler for all cell operations
 * - Creates and maintains dependency relationships
 * - Detects and prevents circular dependencies
 * - Updates dependent cells when source cells change
 * - Handles both direct assignments and range operations
 */
bool handle_dependencies(ParsedCommand *result) {
    // Handle special commands first
    if (result->type == CMD_SCROLL || 
        result->type == CMD_SCROLL_DIR || 
        result->type == CMD_CONTROL || 
        result->type == CMD_SLEEP ||
        result->type == CMD_INVALID) {
        
        process_command(result);
        return false;
    }

    int r1 = result->op1.row - 1;
    int c1 = result->op1.col - 1;

    // Special handling for SLEEP function
    if (result->func == FUNC_SLEEP) {
        // Remove previous dependencies
        Parent *parent = Parent_lst[r1][c1];
        while (parent != NULL) {
            remove_child(parent->r, parent->c, r1, c1);
            parent = parent->next;
        }
        while (Parent_lst[r1][c1] != NULL) {
            remove_parent(Parent_lst[r1][c1]->r, Parent_lst[r1][c1]->c, r1, c1);
        }

        // Process the sleep command directly here
        int sleep_duration;
        if (result->op2.row != 0 || result->op2.col != 0) {
            sleep_duration = sheet[result->op2.row - 1][result->op2.col - 1];
        } else {
            sleep_duration = result->op2.value;
        }
        
        if (sleep_duration >= 0 && sleep_duration <= 3600) {
            sleep(sleep_duration);
            sheet[r1][c1] = sleep_duration;
        } else {
            printf("Error: Sleep duration must be between 0 and 3600 seconds.\n");
            sheet[r1][c1] = 0;
        }
        
        return true;
    }

    // Queries without range (Direct assignments and arithmetic expressions)
    if (result->func == FUNC_NONE && (result->type == CMD_SET_CELL || result->type == CMD_ARITHMETIC)) {
        // Remove r1, c1 from child lists of its parents
        Parent *parent = Parent_lst[r1][c1];
        while (parent != NULL) {
            remove_child(parent->r, parent->c, r1, c1);
            parent = parent->next;
        }

        // Remove all parents of r1, c1 using remove_parent
        while (Parent_lst[r1][c1] != NULL) {
            remove_parent(Parent_lst[r1][c1]->r, Parent_lst[r1][c1]->c, r1, c1);
        }

        // Check if op2 and op3 are values or cells
        bool op2_is_cell = (result->op2.row != 0 || result->op2.col != 0);
        bool op3_is_cell = (result->op3.row != 0 || result->op3.col != 0);

        int val1 = op2_is_cell ? sheet[result->op2.row - 1][result->op2.col - 1] : result->op2.value;
        int val2 = op3_is_cell ? sheet[result->op3.row - 1][result->op3.col - 1] : result->op3.value;

        // Assign dependencies if op2/op3 are cells
        if (op2_is_cell) {
            assign_parent(result->op2.row - 1, result->op2.col - 1, r1, c1, *result);
            assign_child(result->op2.row - 1, result->op2.col - 1, r1, c1, *result);
        }
        if (op3_is_cell) {
            assign_parent(result->op3.row - 1, result->op3.col - 1, r1, c1, *result);
            assign_child(result->op3.row - 1, result->op3.col - 1, r1, c1, *result);
        }

        // Cycle detection
        if (detect_cycle(r1, c1)) {
            if (op2_is_cell) {
                remove_child(result->op2.row - 1, result->op2.col - 1, r1, c1);
                remove_parent(result->op2.row - 1, result->op2.col - 1, r1, c1);
            }
            if (op3_is_cell) {
                remove_child(result->op3.row - 1, result->op3.col - 1, r1, c1);
                remove_parent(result->op3.row - 1, result->op3.col - 1, r1, c1);
            }
            printf("Cycle detected! Assignment aborted.\n");
            return false;
        }
    }

    // Queries with range (Aggregates and operations on ranges)
    else {
        int r_start = result->op2.row - 1;
        int c_start = result->op2.col - 1;
        int r_end = result->op3.row - 1;
        int c_end = result->op3.col - 1;

        // Remove previous dependencies
        Parent *parent = Parent_lst[r1][c1];
        while (parent != NULL) {    
            remove_child(parent->r, parent->c, r1, c1);
            parent = parent->next;
        }

        while (Parent_lst[r1][c1] != NULL) {
            remove_parent(Parent_lst[r1][c1]->r, Parent_lst[r1][c1]->c, r1, c1);
        }

        // Assign new dependencies
        for (int i = r_start; i <= r_end; i++) {
            for (int j = c_start; j <= c_end; j++) {
                assign_parent(i, j, r1, c1, *result);
                assign_child(i, j, r1, c1, *result);
            }
        }

        if (detect_cycle(r1, c1)) {
            for (int i = r_start; i <= r_end; i++) {
                for (int j = c_start; j <= c_end; j++) {
                    remove_child(i, j, r1, c1);
                    remove_parent(i, j, r1, c1);
                }
            }
            printf("Cycle detected! Range-based operation aborted.\n");
            return false;
        } else {
            // Perform the operation based on result->func
            // switch (result->func) {
            //     case FUNC_MIN:
            //         sheet[r1][c1] = compute_min(r_start, c_start, r_end, c_end);
            //         break;
            //     case FUNC_MAX:
            //         sheet[r1][c1] = compute_max(r_start, c_start, r_end, c_end);
            //         break;
            //     case FUNC_AVG:
            //         sheet[r1][c1] = compute_avg(r_start, c_start, r_end, c_end);
            //         break;
            //     case FUNC_SUM:
            //         sheet[r1][c1] = compute_sum(r_start, c_start, r_end, c_end);
            //         break;
            //     case FUNC_STDEV:
            //         sheet[r1][c1] = compute_stdev(r_start, c_start, r_end, c_end);
            //         break;
            //     case FUNC_SLEEP:
            //         sleep(result->sleep_duration);
            //         break;
            //     default:
            //         printf("Unknown function type.\n");
            //         return false;
            //         break;
            // }
        }
    }
    
    // If we've made it here, everything executed correctly
    return true;
}

/**
 * Processes function commands (MIN, MAX, AVG, SUM, STDEV, SLEEP)
 * - Handles range-based operations
 * - Special handling for SLEEP function:
 *   * Supports both cell reference and direct value for duration
 *   * Creates dependencies for cell-based sleep durations
 *   * Validates sleep duration (0-3600 seconds)
 *   * Updates cell value after sleep
 */
void function(ParsedCommand *result) {
    int r1 = result->op1.row - 1;
    int c1 = result->op1.col - 1;
    int r2 = result->op2.row - 1;
    int c2 = result->op2.col - 1;
    int r3 = result->op3.row - 1;
    int c3 = result->op3.col - 1;
    
    // Skip SLEEP function as it's handled in handle_dependencies
    if (result->func == FUNC_SLEEP) {
        return;
    }

    // Initialize variables for range operations
    int sum = 0;
    int count = 0;
    int min = INT_MAX;
    int max = INT_MIN;
    double sum_sq = 0;

    // Calculate range statistics if not SLEEP function
    if (result->func != FUNC_SLEEP) {
        for (int i = r2; i <= r3; i++) {
            for (int j = c2; j <= c3; j++) {
                int value = sheet[i][j];
                sum += value;
                count++;
                if (value < min) min = value;
                if (value > max) max = value;
                sum_sq += (value * value);
            }
        }
    }

    switch (result->func) {
        case FUNC_MIN:
            sheet[r1][c1] = min;
            break;
        case FUNC_MAX:
            sheet[r1][c1] = max;
            break;
        case FUNC_AVG:
            sheet[r1][c1] = (count > 0) ? sum / count : 0;
            break;
        case FUNC_SUM:
            sheet[r1][c1] = sum;
            break;
        case FUNC_STDEV:
            if (count > 1) {
                double mean = (double)sum / count;
                double variance = (sum_sq - count * mean * mean) / (count - 1);
                sheet[r1][c1] = (int)sqrt(variance);
            } else {
                sheet[r1][c1] = 0;
            }
            break;
        case FUNC_NONE:
            break;
    }
    // Handle dependencies
}

/**
 * Main command processor
 * - Routes commands to appropriate handlers
 * - Handles all command types:
 *   * Cell operations (SET, ARITHMETIC, FUNCTION)
 *   * Navigation (SCROLL, SCROLL_DIR)
 *   * Control commands (enable/disable_output)
 *   * Sleep commands
 */
void process_command(ParsedCommand *result) {
    switch (result->type) {
        case CMD_SET_CELL:
            assign(result);
            break;
        case CMD_SCROLL:
            set_org(result->op1.row, result->op1.col);
            break;
        case CMD_SCROLL_DIR:
            // Validate scroll direction before processing
            if (result->scroll_direction == 'w' || 
                result->scroll_direction == 'a' || 
                result->scroll_direction == 's' || 
                result->scroll_direction == 'd') {
                switch (result->scroll_direction) {
                    case 'w': w(); break;
                    case 'a': a(); break;
                    case 's': s(); break;
                    case 'd': d(); break;
                }
            }
            break;
        case CMD_CONTROL:
            if (strcmp(result->control_cmd, "disable_output") == 0) {
                output_enabled = false;
            } else if (strcmp(result->control_cmd, "enable_output") == 0) {
                output_enabled = true;
            }
            break;
        case CMD_SLEEP:
            sleep((unsigned int) result->sleep_duration);
            break;
        case CMD_FUNCTION:
            function(result);
            break;
        case CMD_ARITHMETIC:
            arithmetic(result);
            break;
        case CMD_INVALID:
            // Handle invalid command
            break;
    }
}

/**
 * Checks if the value being assigned is a valid numeric value
 */
bool is_numeric_value(ParsedCommand* cmd) {
    // For direct cell assignments
    if (cmd->type == CMD_SET_CELL) {
        // If it's a cell reference, it's valid
        if (cmd->op2.row != 0 || cmd->op2.col != 0) {
            return true;
        }
        
        // If it's a direct value, check if the expression is numeric
        char* endptr;
        strtol(cmd->expression, &endptr, 10);
        // If endptr points to the end of string, the entire string was numeric
        return (*endptr == '\0');
    }
    return true;  // Other command types are considered valid
}
