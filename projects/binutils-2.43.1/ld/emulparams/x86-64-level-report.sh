PARSE_AND_LIST_OPTIONS_X86_64_LEVEL_REPORT='
  fprintf (file, _("\
  -z isa-level-report=[none|all|needed|used] (default: none)\n\
                              Report x86-64 ISA level\n"));
'
PARSE_AND_LIST_ARGS_CASE_Z_X86_64_LEVEL_REPORT='
      else if (strncmp (optarg, "isa-level-report=", 17) == 0)
	{
	  if (strcmp (optarg + 17, "none") == 0)
	    params.isa_level_report = isa_level_report_none;
	  else if (strcmp (optarg + 17, "all") == 0)
	    params.isa_level_report = (isa_level_report_needed
				       | isa_level_report_used);
	  else if (strcmp (optarg + 17, "needed") == 0)
	    params.isa_level_report = isa_level_report_needed;
	  else if (strcmp (optarg + 17, "used") == 0)
	    params.isa_level_report = isa_level_report_used;
	  else
	    einfo (_("%F%P: invalid option for -z isa-level-report=: %s\n"),
		   optarg + 17);
	}
'

PARSE_AND_LIST_OPTIONS="$PARSE_AND_LIST_OPTIONS $PARSE_AND_LIST_OPTIONS_X86_64_LEVEL_REPORT"
PARSE_AND_LIST_ARGS_CASE_Z="$PARSE_AND_LIST_ARGS_CASE_Z $PARSE_AND_LIST_ARGS_CASE_Z_X86_64_LEVEL_REPORT"
