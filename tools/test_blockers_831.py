"""Direct unit tests for the narrow blocker refinement, not evidence replay."""
from dma_association_831 import refine_blockers
n=0
for other in ('memory-exceptions','unsupported-segments','multiple-descriptor-forms','amd-unresolved-alias-group','amd-device-policy-unresolved','amd-all-device-semantics-unresolved','multiple-include-all'):
    old={'blockers':['unresolved-records',other]}
    assert refine_blockers(old,[{}],0,[])==([other],['unresolved-records']);assert old['blockers']==['unresolved-records',other];n+=1
for classified,remaining,other in (([],0,[]),([{}],1,[]),([{}],0,[{}]),([],1,[{}])):
    old={'blockers':['unresolved-records']}
    assert refine_blockers(old,classified,remaining,other)==(['unresolved-records'],[]);n+=1
assert refine_blockers({'blockers':[]},[{}],0,[]) == ([],[]);n+=1
print(f'{n}/{n} unit tests PASS; other blockers retained')
