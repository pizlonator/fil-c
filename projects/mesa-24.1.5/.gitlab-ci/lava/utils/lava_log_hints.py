from __future__ import annotations

import re
from datetime import datetime, timedelta
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Any, Optional, Sequence

if TYPE_CHECKING:
    from lava.utils import LogFollower

from lava.exceptions import MesaCIKnownIssueException
from lava.utils.console_format import CONSOLE_LOG
from lava.utils.constants import (
    KNOWN_ISSUE_R8152_MAX_CONSECUTIVE_COUNTER,
    LOG_DEBUG_FEEDBACK_NOISE,
    KNOWN_ISSUE_R8152_PATTERNS,
    A6XX_GPU_RECOVERY_WATCH_PERIOD_MIN,
    A6XX_GPU_RECOVERY_FAILURE_MESSAGE,
    A6XX_GPU_RECOVERY_FAILURE_MAX_COUNT,
)
from lava.utils.log_section import LogSectionType


def search_known_issue_patterns(patterns: Sequence[str], line: str) -> str:
    for pattern in patterns:
        if re.search(pattern, line):
            return pattern
    return ""


@dataclass
class LAVALogHints:
    log_follower: LogFollower
    r8152_issue_consecutive_counter: int = field(default=0, init=False)
    reboot_counter: int = field(default=0, init=False)
    a6xx_gpu_recovery_fail_counter: int = field(default=0, init=False)
    a6xx_gpu_first_fail_time: Optional[datetime] = field(default=None, init=False)

    def raise_known_issue(self, message) -> None:
        raise MesaCIKnownIssueException(
            "Found known issue: "
            f"{CONSOLE_LOG['FG_MAGENTA']}"
            f"{message}"
            f"{CONSOLE_LOG['RESET']}"
        )

    def detect_failure(self, new_lines: list[dict[str, Any]]):
        for line in new_lines:
            if line["msg"] == LOG_DEBUG_FEEDBACK_NOISE:
                continue
            self.detect_r8152_issue(line)
            self.detect_forced_reboot(line)
            self.detect_a6xx_gpu_recovery_failure(line)

    def detect_r8152_issue(self, line):
        if self.log_follower.phase in (
            LogSectionType.LAVA_BOOT,
            LogSectionType.TEST_CASE,
        ) and line["lvl"] in ("feedback", "target"):
            if search_known_issue_patterns(KNOWN_ISSUE_R8152_PATTERNS, line["msg"]):
                if (
                    self.r8152_issue_consecutive_counter
                    < KNOWN_ISSUE_R8152_MAX_CONSECUTIVE_COUNTER
                ):
                    self.r8152_issue_consecutive_counter += 1
                    return

                self.raise_known_issue(
                    "Probable network issue failure encountered, retrying the job"
                )

        # Reset the status, as the `nfs... still trying` complaint was not detected
        self.r8152_issue_consecutive_counter = 0

    def detect_forced_reboot(self, line: dict[str, Any]) -> None:
        if (
            self.log_follower.phase == LogSectionType.TEST_CASE
            and line["lvl"] == "feedback"
        ):
            if re.search(r"^Reboot requested", line["msg"]):
                self.reboot_counter += 1

                if self.reboot_counter > 0:
                    self.raise_known_issue(
                        "Forced reboot detected during test phase, failing the job..."
                    )

    # If the a6xx gpu repeatedly fails to recover over a short period of time,
    # then successful recovery is unlikely so cancel the job preemptively.
    def detect_a6xx_gpu_recovery_failure(self, line: dict[str, Any]) -> None:
        if search_known_issue_patterns(A6XX_GPU_RECOVERY_FAILURE_MESSAGE, line["msg"]):
            time_of_failure = datetime.fromisoformat(line["dt"])
            self.a6xx_gpu_recovery_fail_counter += 1

            if self.a6xx_gpu_first_fail_time is None:
                self.a6xx_gpu_first_fail_time = time_of_failure

            if self.a6xx_gpu_recovery_fail_counter == A6XX_GPU_RECOVERY_FAILURE_MAX_COUNT:
                time_since_first_fail = time_of_failure - self.a6xx_gpu_first_fail_time
                if time_since_first_fail <=  timedelta(minutes=A6XX_GPU_RECOVERY_WATCH_PERIOD_MIN):
                    self.raise_known_issue(
                        "Repeated GPU recovery failure detected: cancelling the job"
                    )
                else:
                    self.a6xx_gpu_first_fail_time = None
                    self.a6xx_gpu_recovery_fail_counter = 0
