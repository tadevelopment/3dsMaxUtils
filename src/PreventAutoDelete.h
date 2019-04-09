#pragma once

class PreventAutoDelete
{
	bool m_wasSet;
	ReferenceTarget* m_target;
public:
	DoHoldTarget(ReferenceTarget* target)
		: m_target(target)
	{
		if (target != nullptr)
			m_wasSet = !target->TestAFlag(A_LOCK_TARGET);
		// Prevent the solver from auto-deleting on losing reference
		target->SetAFlag(A_LOCK_TARGET);
	}

	~DoHoldTarget()
	{
		// If we prevented the class from auto-deleting
		if (m_wasSet)
		{
			m_target->ClearAFlag(A_LOCK_TARGET);
			m_target->MaybeAutoDelete();
		}
	}
};