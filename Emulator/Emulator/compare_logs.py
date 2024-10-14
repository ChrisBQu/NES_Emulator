MAX_LINE = 5003
GROUND_TRUTH_LOG = "goodlog.txt"
TEST_LOG = "log.txt"

line_num = 0
with open(TEST_LOG, 'r') as bad_log, open(GROUND_TRUTH_LOG, 'r') as good_log:
    for line_b, line_g in zip(bad_log, good_log):
        line_num += 1
        if line_b != line_g and line_num != MAX_LINE:
            print(f"Difference found at line: {line_num}")
            print(f"Log:\n{line_b}")
            print(f"Ground Truth:\n{line_g}")
            break
    else:
        if line_num == 5003:
            print("No differences for any legal opcode.")
