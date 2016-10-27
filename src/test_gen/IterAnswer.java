package com.diffblue.java_testcase;

import org.mockito.stubbing.Answer;
import org.mockito.invocation.InvocationOnMock;
import java.util.ArrayList;

public class IterAnswer<T> implements Answer<T> {

    private String classname;
    private String methodname;
    private int idx = 0;
    private ArrayList<T> answers;
    private ArrayList<Object[]> expectedParameters;

    public IterAnswer(String cn, String mn, ArrayList<T> _answers, ArrayList<Object[]> eps) {
        classname = cn;
        methodname = mn;
	answers = _answers;
	expectedParameters = eps;
    }
    
    public T answer(InvocationOnMock invocation) {
        if(idx == answers.size())
        {
          System.out.println("WARNING: more answers than in trace " +
                             (idx + 1) +
                             " instead of just " + idx +
                             " will restart with first");
          idx = 0;
        }

	T result = answers.get(idx);
	
	if(expectedParameters.size() != 0)
	{
	  Object[] expected = expectedParameters.get(idx);
	  Object[] actual = invocation.getArguments();
	  assert(expected.length==actual.length);
	  for(int i = 0; i < expected.length; ++i)
	  {
	    try {
	      CompareWithFieldList.compare(actual[i], expected[i], "");
	    }
	    catch(UnexpectedMockParameterException e) {
	      String errormsg="Mocked class " + classname +
		" method " + methodname +
		" invocation " + (idx+1) +
		" parameter " + (i+1) +
		": " + e.getMessage();
	      throw new UnexpectedMockParameterException(errormsg);
	    }
	  }
	}
	
        idx++;

	return result;
    }    

}


